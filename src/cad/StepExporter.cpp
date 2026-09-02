#include "cad/StepExporter.hpp"

#include "core/Image.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFill.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_Sewing.hxx>
#include <BRep_Tool.hxx>
#include <DESTEP_Parameters.hxx>
#include <BRepTools.hxx>
#include <GeomAPI_PointsToBSplineSurface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>

namespace rf {
namespace {

std::size_t targetLongestEdge(StepSurfaceQuality quality) {
    switch (quality) {
    case StepSurfaceQuality::Draft: return 16;
    case StepSurfaceQuality::Standard: return 32;
    case StepSurfaceQuality::Fine: return 64;
    case StepSurfaceQuality::VeryFine: return 96;
    }
    return 32;
}

std::pair<std::size_t, std::size_t> fitDimensions(
    const HeightField& field,
    StepSurfaceQuality quality) {
    const auto longest = targetLongestEdge(quality);
    const auto scale = std::min(1.0, longest / static_cast<double>(std::max(field.columns(), field.rows())));
    return {
        std::max<std::size_t>(4, std::lround(field.columns() * scale)),
        std::max<std::size_t>(4, std::lround(field.rows() * scale)),
    };
}

double bilinearHeight(const HeightField& field, double u, double v) {
    const auto sourceX = std::clamp(u, 0.0, 1.0) * (field.columns() - 1);
    const auto sourceY = std::clamp(v, 0.0, 1.0) * (field.rows() - 1);
    const auto x0 = static_cast<std::size_t>(sourceX);
    const auto y0 = static_cast<std::size_t>(sourceY);
    const auto x1 = std::min(x0 + 1, field.columns() - 1);
    const auto y1 = std::min(y0 + 1, field.rows() - 1);
    const auto fx = sourceX - x0;
    const auto fy = sourceY - y0;
    const auto top = field.at(x0, y0) * (1.0 - fx) + field.at(x1, y0) * fx;
    const auto bottom = field.at(x0, y1) * (1.0 - fx) + field.at(x1, y1) * fx;
    return top * (1.0 - fy) + bottom * fy;
}

} // namespace

Result<StepExportReport> StepExporter::write(
    const HeightField& field,
    const std::filesystem::path& path,
    const StepExportOptions& options) {
    if (options.surfaceToleranceMm <= 0.0 || options.sewingToleranceMm <= 0.0) {
        return Error{"STEP surface and sewing tolerances must be positive."};
    }
    const auto [columns, rows] = fitDimensions(field, options.quality);
    TColgp_Array2OfPnt points(1, static_cast<Standard_Integer>(columns),
                             1, static_cast<Standard_Integer>(rows));
    for (std::size_t x = 0; x < columns; ++x) {
        const auto u = x / static_cast<double>(columns - 1);
        for (std::size_t y = 0; y < rows; ++y) {
            const auto v = y / static_cast<double>(rows - 1);
            points.SetValue(
                static_cast<Standard_Integer>(x + 1),
                static_cast<Standard_Integer>(y + 1),
                gp_Pnt(
                    -field.dimensions().widthMm * 0.5 + u * field.dimensions().widthMm,
                    -field.dimensions().heightMm * 0.5 + v * field.dimensions().heightMm,
                    bilinearHeight(field, u, v)));
        }
    }

    GeomAPI_PointsToBSplineSurface fitter(
        points,
        3,
        8,
        GeomAbs_C2,
        options.surfaceToleranceMm);
    const Handle(Geom_BSplineSurface) surface = fitter.Surface();
    if (surface.IsNull()) {
        return Error{
            "STEP generation could not fit a B-spline surface at the selected tolerance. "
            "Try a coarser STEP quality or larger surface tolerance."};
    }

    const TopoDS_Face topFace = BRepBuilderAPI_MakeFace(surface, options.sewingToleranceMm);
    const auto halfWidth = field.dimensions().widthMm * 0.5;
    const auto halfHeight = field.dimensions().heightMm * 0.5;
    TopoDS_Face bottomFace = BRepBuilderAPI_MakeFace(
        gp_Pln(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, -1.0)),
        -halfWidth,
        halfWidth,
        -halfHeight,
        halfHeight);

    BRepOffsetAPI_Sewing sewing(options.sewingToleranceMm);
    sewing.Add(topFace);
    sewing.Add(bottomFace);
    for (TopExp_Explorer explorer(topFace, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const auto topEdge = TopoDS::Edge(explorer.Current());
        TopoDS_Vertex firstVertex;
        TopoDS_Vertex lastVertex;
        TopExp::Vertices(topEdge, firstVertex, lastVertex, Standard_True);
        const auto first = BRep_Tool::Pnt(firstVertex);
        const auto last = BRep_Tool::Pnt(lastVertex);
        BRepBuilderAPI_MakeEdge bottomEdgeBuilder(
            gp_Pnt(first.X(), first.Y(), 0.0),
            gp_Pnt(last.X(), last.Y(), 0.0));
        const TopoDS_Edge bottomEdge = bottomEdgeBuilder.Edge();
        sewing.Add(BRepFill::Face(topEdge, bottomEdge));
    }
    sewing.Perform();
    const auto sewn = sewing.SewedShape();
    if (sewn.IsNull()) {
        return Error{"STEP surface fitting succeeded, but the boundary faces could not be sewn."};
    }

    TopoDS_Shell shell;
    for (TopExp_Explorer explorer(sewn, TopAbs_SHELL); explorer.More(); explorer.Next()) {
        shell = TopoDS::Shell(explorer.Current());
        break;
    }
    if (shell.IsNull() && sewn.ShapeType() == TopAbs_SHELL) {
        shell = TopoDS::Shell(sewn);
    }
    if (shell.IsNull()) {
        return Error{
            "STEP generation produced faces but not a closed shell. Increase the sewing tolerance "
            "or choose a coarser surface quality."};
    }

    TopoDS_Solid solid = BRepBuilderAPI_MakeSolid(shell);
    BRepLib::OrientClosedSolid(solid);
    if (!BRepCheck_Analyzer(solid, Standard_True).IsValid()) {
        return Error{
            "STEP generation produced a shell that OpenCascade could not validate as a solid. "
            "Try Standard quality or a larger surface tolerance."};
    }

    DESTEP_Parameters transferParameters;
    transferParameters.WriteProductName = options.modelName.c_str();
    transferParameters.WriteUnit = UnitsMethods_LengthUnit_Millimeter;
    transferParameters.WriteTessellated = DESTEP_Parameters::RWMode_Tessellated_Off;
    switch (options.schema) {
    case StepSchema::AP203:
        transferParameters.WriteSchema = DESTEP_Parameters::WriteMode_StepSchema_AP203;
        break;
    case StepSchema::AP214:
        transferParameters.WriteSchema = DESTEP_Parameters::WriteMode_StepSchema_AP214IS;
        break;
    case StepSchema::AP242:
        transferParameters.WriteSchema = DESTEP_Parameters::WriteMode_StepSchema_AP242DIS;
        break;
    }
    STEPControl_Writer writer;
    if (writer.Transfer(solid, STEPControl_AsIs, transferParameters) != IFSelect_RetDone) {
        return Error{"OpenCascade could not transfer the fitted solid into a STEP model."};
    }
    if (writer.Write(path.string().c_str()) != IFSelect_RetDone) {
        return Error{"OpenCascade failed while writing the STEP file: " + path.string()};
    }

    double maximumDeviation{};
    Standard_Real firstU{};
    Standard_Real lastU{};
    Standard_Real firstV{};
    Standard_Real lastV{};
    surface->Bounds(firstU, lastU, firstV, lastV);
    for (std::size_t x = 0; x < columns; ++x) {
        const auto u = firstU + (lastU - firstU) * x / static_cast<double>(columns - 1);
        for (std::size_t y = 0; y < rows; ++y) {
            const auto v = firstV + (lastV - firstV) * y / static_cast<double>(rows - 1);
            const auto point = surface->Value(u, v);
            const auto sourceHeight = bilinearHeight(
                field,
                x / static_cast<double>(columns - 1),
                y / static_cast<double>(rows - 1));
            maximumDeviation = std::max(maximumDeviation, std::abs(point.Z() - sourceHeight));
        }
    }
    return StepExportReport{columns, rows, maximumDeviation};
}

} // namespace rf
