/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>

#include <vcl/skia/SkiaHelper.hxx>
#include <skia/salbmp.hxx>
#include <bitmapwriteaccess.hxx>
#include <vcl/virdev.hxx>
#include <tools/stream.hxx>
#include <vcl/graphicfilter.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>

// This tests backends that use Skia (i.e. intentionally not the svp one, which is the default.)
// Note that you still may need to actually set for Skia to be used (see vcl/README.vars).
// If Skia is not enabled, all tests will be silently skipped.
namespace
{
class SkiaTest : public test::BootstrapFixture
{
public:
    SkiaTest()
        : test::BootstrapFixture(true, false)
    {
    }

    void testBitmapErase();
    void testDrawShaders();
    void testInterpretAs8Bit();
    void testAlphaBlendWith();

    CPPUNIT_TEST_SUITE(SkiaTest);
    CPPUNIT_TEST(testBitmapErase);
    CPPUNIT_TEST(testDrawShaders);
    CPPUNIT_TEST(testInterpretAs8Bit);
    CPPUNIT_TEST(testAlphaBlendWith);
    CPPUNIT_TEST_SUITE_END();

private:
#if 0
    template <class BitmapT> // handle both Bitmap and BitmapEx
    void savePNG(const OUString& sWhere, const BitmapT& rBmp)
    {
        SvFileStream aStream(sWhere, StreamMode::WRITE | StreamMode::TRUNC);
        GraphicFilter& rFilter = GraphicFilter::GetGraphicFilter();
        rFilter.compressAsPNG(BitmapEx(rBmp), aStream);
    }
    void savePNG(const OUString& sWhere, const ScopedVclPtr<VirtualDevice>& device)
    {
        SvFileStream aStream(sWhere, StreamMode::WRITE | StreamMode::TRUNC);
        GraphicFilter& rFilter = GraphicFilter::GetGraphicFilter();
        rFilter.compressAsPNG(device->GetBitmapEx(Point(), device->GetOutputSizePixel()), aStream);
    }
#endif
};

void SkiaTest::testBitmapErase()
{
    if (!SkiaHelper::isVCLSkiaEnabled())
        return;
    Bitmap bitmap(Size(10, 10), 24);
    SkiaSalBitmap* skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaBitmap);
    // Uninitialized bitmap.
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasBuffer());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasImage());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasAlphaImage());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasEraseColor());
    // Test that Bitmap.Erase() just sets erase color and doesn't allocate pixels.
    bitmap.Erase(COL_RED);
    skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasBuffer());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasImage());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasAlphaImage());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasEraseColor());
    // Reading a pixel will create pixel data.
    CPPUNIT_ASSERT_EQUAL(BitmapColor(COL_RED), BitmapReadAccess(bitmap).GetColor(0, 0));
    skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasBuffer());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasImage());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasAlphaImage());
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasEraseColor());
}

// Test that draw calls that internally result in SkShader calls work properly.
void SkiaTest::testDrawShaders()
{
    if (!SkiaHelper::isVCLSkiaEnabled())
        return;
    ScopedVclPtr<VirtualDevice> device = VclPtr<VirtualDevice>::Create(DeviceFormat::DEFAULT);
    device->SetOutputSizePixel(Size(20, 20));
    device->SetBackground(Wallpaper(COL_WHITE));
    device->Erase();
    Bitmap bitmap(Size(10, 10), 24);
    bitmap.Erase(COL_RED);
    SkiaSalBitmap* skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaBitmap->PreferSkShader());
    AlphaMask alpha(Size(10, 10));
    alpha.Erase(64);
    SkiaSalBitmap* skiaAlpha = dynamic_cast<SkiaSalBitmap*>(alpha.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaAlpha->PreferSkShader());

    device->DrawBitmap(Point(5, 5), bitmap);
    //savePNG("/tmp/a1.png", device);
    // Check that the area is painted, but nothing else.
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, device->GetPixel(Point(0, 0)));
    CPPUNIT_ASSERT_EQUAL(COL_RED, device->GetPixel(Point(5, 5)));
    CPPUNIT_ASSERT_EQUAL(COL_RED, device->GetPixel(Point(14, 14)));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, device->GetPixel(Point(15, 15)));
    device->Erase();

    device->DrawBitmapEx(Point(5, 5), BitmapEx(bitmap, alpha));
    //savePNG("/tmp/a2.png", device);
    Color resultRed(COL_RED.GetRed() * 3 / 4 + 64, 64, 64); // 3/4 red, 1/4 white
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, device->GetPixel(Point(0, 0)));
    CPPUNIT_ASSERT_EQUAL(resultRed, device->GetPixel(Point(5, 5)));
    CPPUNIT_ASSERT_EQUAL(resultRed, device->GetPixel(Point(14, 14)));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, device->GetPixel(Point(15, 15)));
    device->Erase();

    basegfx::B2DHomMatrix matrix;
    matrix.scale(10, 10);
    matrix.rotate(M_PI / 4);
    device->DrawTransformedBitmapEx(matrix, BitmapEx(bitmap, alpha));
    //savePNG("/tmp/a3.png", device);
    CPPUNIT_ASSERT_EQUAL(resultRed, device->GetPixel(Point(0, 1)));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, device->GetPixel(Point(1, 0)));
    CPPUNIT_ASSERT_EQUAL(resultRed, device->GetPixel(Point(0, 10)));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, device->GetPixel(Point(10, 10)));
    device->Erase();

    // Test with scaling. Use everything 10x larger to reduce the impact of smoothscaling.
    ScopedVclPtr<VirtualDevice> deviceLarge = VclPtr<VirtualDevice>::Create(DeviceFormat::DEFAULT);
    deviceLarge->SetOutputSizePixel(Size(200, 200));
    deviceLarge->SetBackground(Wallpaper(COL_WHITE));
    deviceLarge->Erase();
    Bitmap bitmapLarge(Size(100, 100), 24);
    bitmapLarge.Erase(COL_RED);
    SkiaSalBitmap* skiaBitmapLarge
        = dynamic_cast<SkiaSalBitmap*>(bitmapLarge.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaBitmapLarge->PreferSkShader());
    AlphaMask alphaLarge(Size(100, 100));
    alphaLarge.Erase(64);
    {
        BitmapWriteAccess access(bitmapLarge);
        access.SetFillColor(COL_BLUE);
        access.FillRect(tools::Rectangle(Point(20, 40), Size(10, 10)));
    }
    // Using alpha will still lead to shaders being used.
    deviceLarge->DrawBitmapEx(Point(50, 50), Size(60, 60), Point(20, 20), Size(30, 30),
                              BitmapEx(bitmapLarge, alphaLarge));
    //savePNG("/tmp/a4.png", deviceLarge);
    Color resultBlue(64, 64, COL_BLUE.GetBlue() * 3 / 4 + 64); // 3/4 blue, 1/4 white
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, deviceLarge->GetPixel(Point(40, 40)));
    CPPUNIT_ASSERT_EQUAL(resultRed, deviceLarge->GetPixel(Point(50, 50)));
    CPPUNIT_ASSERT_EQUAL(resultRed, deviceLarge->GetPixel(Point(100, 100)));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, deviceLarge->GetPixel(Point(110, 110)));
    // Don't test colors near the edge between the colors, smoothscaling affects them.
    const int diff = 3;
    CPPUNIT_ASSERT_EQUAL(resultRed, deviceLarge->GetPixel(Point(50 + diff, 89 - diff)));
    CPPUNIT_ASSERT_EQUAL(resultBlue, deviceLarge->GetPixel(Point(50 + diff, 90 + diff)));
    CPPUNIT_ASSERT_EQUAL(resultBlue, deviceLarge->GetPixel(Point(69 - diff, 100 - diff)));
    CPPUNIT_ASSERT_EQUAL(resultRed, deviceLarge->GetPixel(Point(70 + diff, 100 - diff)));
    CPPUNIT_ASSERT_EQUAL(resultBlue, deviceLarge->GetPixel(Point(50 + diff, 100 - diff)));
    device->Erase();
}

void SkiaTest::testInterpretAs8Bit()
{
    if (!SkiaHelper::isVCLSkiaEnabled())
        return;
    Bitmap bitmap(Size(10, 10), 24);
    // Test with erase color.
    bitmap.Erase(Color(33, 33, 33));
    SkiaSalBitmap* skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasEraseColor());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt16>(24), bitmap.GetBitCount());
    bitmap.Convert(BmpConversion::N8BitNoConversion);
    skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasEraseColor());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt16>(8), bitmap.GetBitCount());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt8>(33), BitmapReadAccess(bitmap).GetPixelIndex(0, 0));

    // Test with image.
    bitmap = Bitmap(Size(10, 10), 24);
    bitmap.Erase(Color(34, 34, 34));
    BitmapReadAccess(bitmap).GetColor(0, 0); // Create pixel data, reset erase color.
    skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    skiaBitmap->GetSkImage();
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasEraseColor());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasImage());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt16>(24), bitmap.GetBitCount());
    bitmap.Convert(BmpConversion::N8BitNoConversion);
    skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasImage());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt16>(8), bitmap.GetBitCount());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt8>(34), BitmapReadAccess(bitmap).GetPixelIndex(0, 0));
}

void SkiaTest::testAlphaBlendWith()
{
    if (!SkiaHelper::isVCLSkiaEnabled())
        return;
    AlphaMask alpha(Size(10, 10));
    Bitmap bitmap(Size(10, 10), 24);
    // Test with erase colors set.
    alpha.Erase(64);
    SkiaSalBitmap* skiaAlpha = dynamic_cast<SkiaSalBitmap*>(alpha.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaAlpha->unittestHasEraseColor());
    bitmap.Erase(Color(64, 64, 64));
    SkiaSalBitmap* skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasEraseColor());
    alpha.BlendWith(bitmap);
    skiaAlpha = dynamic_cast<SkiaSalBitmap*>(alpha.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaAlpha->unittestHasEraseColor());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt16>(8), alpha.GetBitCount());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt8>(112),
                         AlphaMask::ScopedReadAccess(alpha)->GetPixelIndex(0, 0));

    // Test with images set.
    alpha.Erase(64);
    AlphaMask::ScopedReadAccess(alpha)->GetColor(0, 0); // Reading a pixel will create pixel data.
    skiaAlpha = dynamic_cast<SkiaSalBitmap*>(alpha.ImplGetSalBitmap().get());
    skiaAlpha->GetSkImage();
    CPPUNIT_ASSERT(!skiaAlpha->unittestHasEraseColor());
    CPPUNIT_ASSERT(skiaAlpha->unittestHasImage());
    bitmap.Erase(Color(64, 64, 64));
    Bitmap::ScopedReadAccess(bitmap)->GetColor(0, 0); // Reading a pixel will create pixel data.
    skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    skiaBitmap->GetSkImage();
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasEraseColor());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasImage());
    alpha.BlendWith(bitmap);
    skiaAlpha = dynamic_cast<SkiaSalBitmap*>(alpha.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaAlpha->unittestHasImage());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt16>(8), alpha.GetBitCount());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt8>(112),
                         AlphaMask::ScopedReadAccess(alpha)->GetPixelIndex(0, 0));

    // Test with erase color for alpha and image for other bitmap.
    alpha.Erase(64);
    skiaAlpha = dynamic_cast<SkiaSalBitmap*>(alpha.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaAlpha->unittestHasEraseColor());
    bitmap.Erase(Color(64, 64, 64));
    Bitmap::ScopedReadAccess(bitmap)->GetColor(0, 0); // Reading a pixel will create pixel data.
    skiaBitmap = dynamic_cast<SkiaSalBitmap*>(bitmap.ImplGetSalBitmap().get());
    skiaBitmap->GetSkImage();
    CPPUNIT_ASSERT(!skiaBitmap->unittestHasEraseColor());
    CPPUNIT_ASSERT(skiaBitmap->unittestHasImage());
    alpha.BlendWith(bitmap);
    skiaAlpha = dynamic_cast<SkiaSalBitmap*>(alpha.ImplGetSalBitmap().get());
    CPPUNIT_ASSERT(skiaAlpha->unittestHasImage());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt16>(8), alpha.GetBitCount());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_uInt8>(112),
                         AlphaMask::ScopedReadAccess(alpha)->GetPixelIndex(0, 0));
}

} // namespace

CPPUNIT_TEST_SUITE_REGISTRATION(SkiaTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
