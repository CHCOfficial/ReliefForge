#include "TestHarness.hpp"
#include "core/HeightField.hpp"
#include "core/Image.hpp"

RF_TEST("RGBA luminance and alpha mapping") {
    const std::vector<rf::RgbaPixel> pixels{{255, 255, 255, 128}, {0, 0, 0, 255}};
    const auto regular = rf::GrayImage::fromRgba(2, 1, pixels, false);
    const auto alpha = rf::GrayImage::fromRgba(2, 1, pixels, true);
    RF_REQUIRE_NEAR(regular.at(0, 0), 1.0, 1.0e-6);
    RF_REQUIRE_NEAR(alpha.at(0, 0), 128.0 / 255.0, 1.0e-6);
    RF_REQUIRE_NEAR(regular.at(1, 0), 0.0, 1.0e-6);
}

RF_TEST("Image processing supports levels gamma and inversion") {
    rf::GrayImage image(3, 2);
    image.at(0, 0) = 0.25F;
    image.at(1, 0) = 0.5F;
    image.at(2, 0) = 0.75F;
    image.at(0, 1) = 0.25F;
    image.at(1, 1) = 0.5F;
    image.at(2, 1) = 0.75F;
    rf::ImageProcessingParameters parameters;
    parameters.blackLevel = 0.25;
    parameters.whiteLevel = 0.75;
    parameters.invert = true;
    const auto processed = rf::ImageProcessor::process(image, parameters);
    RF_REQUIRE_NEAR(processed.at(0, 0), 1.0, 1.0e-6);
    RF_REQUIRE_NEAR(processed.at(1, 0), 0.5, 1.0e-6);
    RF_REQUIRE_NEAR(processed.at(2, 0), 0.0, 1.0e-6);
}

RF_TEST("Linear height mapping produces physical base and relief depth") {
    rf::GrayImage image(2, 2);
    image.at(0, 0) = 0.0F;
    image.at(1, 0) = 1.0F;
    image.at(0, 1) = 0.5F;
    image.at(1, 1) = 0.25F;
    rf::HeightFieldParameters parameters;
    parameters.dimensions.baseThicknessMm = 2.0;
    parameters.dimensions.reliefDepthMm = 4.0;
    const auto field = rf::HeightField::fromImage(image, parameters);
    RF_REQUIRE_NEAR(field.at(0, 0), 2.0, 1.0e-9);
    RF_REQUIRE_NEAR(field.at(1, 0), 6.0, 1.0e-9);
    RF_REQUIRE_NEAR(field.at(0, 1), 4.0, 1.0e-9);
}

RF_TEST("Bilinear resizing preserves a horizontal gradient") {
    rf::GrayImage image(2, 2);
    image.at(0, 0) = image.at(0, 1) = 0.0F;
    image.at(1, 0) = image.at(1, 1) = 1.0F;
    const auto resized = rf::ImageProcessor::resized(image, 3, 3);
    RF_REQUIRE_NEAR(resized.at(1, 1), 0.5, 1.0e-6);
}

