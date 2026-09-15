// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include "pch.h"
#include "Item.h"
#include "TreeMapLayout.h"

//
// CColorSpace. Helper class for manipulating colors. Static members only.
//
class CColorSpace final
{
public:
    static constexpr double GraphPaletteBrightness = 0.6;
    static constexpr DWORD GraphColorDarker = 0x01000000;
    static constexpr DWORD GraphColorLighter = 0x02000000;
    static constexpr DWORD GraphColorMask = GraphColorDarker | GraphColorLighter;

    static constexpr COLORREF DimColor(const COLORREF color, float factor = 0.9f) noexcept
    {
        factor = std::clamp(factor, 0.0f, 1.0f);
        return RGB(static_cast<BYTE>(GetRValue(color) * factor),
            static_cast<BYTE>(GetGValue(color) * factor),
            static_cast<BYTE>(GetBValue(color) * factor));
    }

    // Returns the arithmetic brightness used by MakeBrightColor.
    static constexpr double GetColorBrightness(const COLORREF color)
    {
        const unsigned int intensity = GetRValue(color) + GetGValue(color) + GetBValue(color);
        return intensity / 255.0 / 3.0;
    }

    // Returns the WCAG relative luminance of an sRGB color (0.0 .. 1.0).
    // This tracks perceived brightness and should be used for contrast choices.
    static double GetRelativeLuminance(const COLORREF color)
    {
        const auto toLinear = [](const BYTE component)
        {
            const double srgb = component / 255.0;
            return srgb <= 0.04045
                ? srgb / 12.92
                : std::pow((srgb + 0.055) / 1.055, 2.4);
        };

        return 0.2126 * toLinear(GetRValue(color))
            + 0.7152 * toLinear(GetGValue(color))
            + 0.0722 * toLinear(GetBValue(color));
    }

    // Gives a color a defined brightness.
    static constexpr COLORREF MakeBrightColor(COLORREF color, double brightness)
    {
        ASSERT(brightness >= 0.0);
        ASSERT(brightness <= 1.0);

        double dred = (GetRValue(color) & 0xFF) / 255.0;
        double dgreen = (GetGValue(color) & 0xFF) / 255.0;
        double dblue = (GetBValue(color) & 0xFF) / 255.0;

        const double f = 3.0 * brightness / (dred + dgreen + dblue);
        dred *= f;
        dgreen *= f;
        dblue *= f;

        int red = static_cast<int>(dred * 255);
        int green = static_cast<int>(dgreen * 255);
        int blue = static_cast<int>(dblue * 255);

        NormalizeColor(red, green, blue);

        return RGB(red, green, blue);
    }

    static constexpr COLORREF ApplyGraphColorFlags(const DWORD rawColor)
    {
        const DWORD flags = rawColor & GraphColorMask;
        COLORREF color = rawColor & 0x00FFFFFF;
        if (flags != GraphColorDarker && flags != GraphColorLighter) return color;

        color = MakeBrightColor(color, GraphPaletteBrightness);
        if (flags == GraphColorDarker) return DimColor(color, 0.66f);
        return RGB(std::min(255, GetRValue(color) + 60),
            std::min(255, GetGValue(color) + 60),
            std::min(255, GetBValue(color) + 60));
    }

    // Swaps values above 255 to the other two values
    static constexpr void NormalizeColor(int& red, int& green, int& blue)
    {
        ASSERT(red + green + blue <= 3 * 255);

        if (red > 255)
        {
            DistributeFirst(red, green, blue);
        }
        else if (green > 255)
        {
            DistributeFirst(green, red, blue);
        }
        else if (blue > 255)
        {
            DistributeFirst(blue, red, green);
        }
    }

protected:
    // Helper function for NormalizeColor()
    static constexpr void DistributeFirst(int& first, int& second, int& third)
    {
        const int h = (first - 255) / 2;
        first = 255;
        second += h;
        third += h;

        if (second > 255)
        {
            const int j = second - 255;
            second = 255;
            third += j;
            ASSERT(third <= 255);
        }
        else if (third > 255)
        {
            const int j = third - 255;
            third = 255;
            second += j;
            ASSERT(second <= 255);
        }
    }
};

//
// CTreeMap. Can create a treemap using rows, squarified, Hilbert, or Moore layouts.
//
// This class is fairly reusable.
//
class CTreeMap final
{
public:
    // Geometry produced by the most recent layout. This is deliberately kept
    // outside CItem so hidden/pruned descendants can never expose rectangles
    // left over from a previous render generation.
    struct VisibleItem
    {
        CItem* item = nullptr;
        CRect rectangle;
        int depth = 0;
    };

    // One of these flags can be added to the COLORREF returned
    // by TmiGetGraphColor(). Used for <Free space> (darker)
    // and <Unknown> (brighter).
    //
    static constexpr DWORD COLORFLAG_DARKER  = CColorSpace::GraphColorDarker;
    static constexpr DWORD COLORFLAG_LIGHTER = CColorSpace::GraphColorLighter;
    static constexpr DWORD COLORFLAG_MASK    = CColorSpace::GraphColorMask;

    //
    // Collection of all treemap options.
    //
    struct Options
    {
        TreeMapLayout::Style style; // Child layout algorithm
        bool grid;           // Whether to draw grid lines
        bool showExtensions; // Whether to show file extensions in treemap
        bool showFolderFrames; // Whether to draw folder borders and headers
        int folderFramesDrawThreshold; // Minimum folder rectangle edge to draw frames
        COLORREF gridColor;  // Color of grid lines
        double brightness;   // 0..1.0   (default = 0.84)
        double height;       // >= 0.0    (default = 0.40)    Factor "H"
        double scaleFactor;  // 0..1.0   (default = 0.90)    Factor "F"
        double ambientLight; // 0..1.0   (default = 0.15)    Factor "Ia"
        double lightSourceX; // -4.0..+4.0 (default = -1.0), negative = left
        double lightSourceY; // -4.0..+4.0 (default = -1.0), negative = top

        constexpr int GetBrightnessPercent() const { return RoundDouble(brightness * 100); }
        constexpr int GetHeightPercent() const { return RoundDouble(height * 100); }
        constexpr int GetScaleFactorPercent() const { return RoundDouble(scaleFactor * 100); }
        constexpr int GetAmbientLightPercent() const { return RoundDouble(ambientLight * 100); }
        constexpr int GetLightSourceXPercent() const { return RoundDouble(lightSourceX * 100); }
        constexpr int GetLightSourceYPercent() const { return RoundDouble(lightSourceY * 100); }
        CPoint GetLightSourcePoint() const { return { GetLightSourceXPercent(), GetLightSourceYPercent() }; }

        constexpr void SetBrightnessPercent(int n) { brightness = n / 100.0; }
        constexpr void SetHeightPercent(int n) { height = n / 100.0; }
        constexpr void SetScaleFactorPercent(int n) { scaleFactor = n / 100.0; }
        constexpr void SetAmbientLightPercent(int n) { ambientLight = n / 100.0; }
        constexpr void SetLightSourceXPercent(int n) { lightSourceX = n / 100.0; }
        constexpr void SetLightSourceYPercent(int n) { lightSourceY = n / 100.0; }
        void SetLightSourcePoint(CPoint pt) { SetLightSourceXPercent(pt.x); SetLightSourceYPercent(pt.y); }

        static constexpr int RoundDouble(double d) { return static_cast<int>(d + (d < 0.0 ? -0.5 : 0.5)); }
    };

    // Get 64 distinct dark grayscale colors
    static void GetDefaultPalette(std::vector<COLORREF>& palette);
    static void GetPalette(std::vector<COLORREF>& palette, bool grayscale);

    // Build the small demo tree used by treemap previews.
    [[nodiscard]] static std::unique_ptr<CItem> BuildDemoTree(bool grayscale);

    // Good values
    static Options GetDefaults();
    static Options GetOriginalDefaults();

    // Construct the treemap generator and register the callback interface.
    CTreeMap();

    // Alter the options
    void SetOptions(const Options* options);
    Options GetOptions() const;

#ifdef _DEBUG
    // DEBUG function
    void RecurseCheckTree(const CItem *item);
#endif // _DEBUG

    // Create and draw a treemap
    void DrawTreeMap(CDC* pdc, CRect rc, CItem* root, const Options* options = nullptr);

    // In the resulting treemap, find the item below a given coordinate.
    // Return value can be nullptr, iff point is outside root rect.
    CItem* FindItemByPoint(CItem* item, CPoint point) const;

    // Access and clear only geometry from the most recent treemap render.
    [[nodiscard]] bool HasValidLayout(const CItem* root) const;
    [[nodiscard]] bool TryGetItemRectangle(const CItem* item, CRect& rectangle) const;
    [[nodiscard]] std::span<const VisibleItem> GetVisibleItems() const { return m_visibleItems; }
    void ClearLayout();
    void TrimMemory();

    // Draws a sample rectangle in the given style (for color legend)
    void DrawColorPreview(CDC* pdc, const CRect& rc, COLORREF color, const Options* options = nullptr);

protected:

    struct BitmapView
    {
        COLORREF* bits;
        std::size_t stride;
    };

    // Returns true, if height and scaleFactor are > 0 and ambientLight is < 1.0
    bool IsCushionShading() const;

    // Leaves space for grid and then calls RenderRectangle()
    void RenderLeaf(BitmapView bitmap, const CItem* item,
        const CRect& rectangle, const std::array<double, 4>& surface) const;

    // Either calls DrawCushion() or DrawSolidRect()
    void RenderRectangle(BitmapView bitmap, const CRect& rc, const std::array<double, 4>& surface, DWORD color) const;

    // Renders cushion pixels.
    void DrawCushion(BitmapView bitmap, const CRect& rc, const std::array<double, 4>& surface, COLORREF col, double brightness) const;

    // Fills solid pixels.
    void DrawSolidRect(BitmapView bitmap, const CRect& rc, COLORREF col, double brightness) const;

    // Adds a new ridge to surface
    static void AddRidge(const CRect& rc, std::array<double, 4>& surface, double h);

    // Draws file extension/filename labels on leaf items
    void DrawTreeMapLabels(CDC* pdc, const CPoint& offset) const;

    void AddVisibleItem(CItem* item, const CRect& rectangle, int depth);
    void BuildHitTestIndex();

    // Default tree map options
    static constexpr Options DefaultOptions = {
        .style = TreeMapLayout::Style::Rows,
        .grid = true,
        .showExtensions = false,
        .showFolderFrames = false,
        .folderFramesDrawThreshold = 5,
        .gridColor = RGB(0, 0, 0),
        .brightness = 0.6,
        .height = 0.0,
        .scaleFactor = 0.91,
        .ambientLight = 0.13,
        .lightSourceX = -1.0,
        .lightSourceY = -1.0
    };

    // Dark grayscale palette. Keep distinct luminance; do not normalize colors.
    static constexpr COLORREF DefaultCushionColors[] = {
        RGB( 80,  80,  80),
        RGB( 48,  48,  48),
        RGB( 96,  96,  96),
        RGB( 64,  64,  64),
        RGB( 72,  72,  72),
        RGB( 40,  40,  40),
        RGB( 88,  88,  88),
        RGB( 56,  56,  56),
        RGB( 84,  84,  84),
        RGB( 52,  52,  52),
        RGB(100, 100, 100),
        RGB( 68,  68,  68),
        RGB( 76,  76,  76),
        RGB( 44,  44,  44),
        RGB( 92,  92,  92),
        RGB( 60,  60,  60),
        RGB( 82,  82,  82),
        RGB( 50,  50,  50),
        RGB( 98,  98,  98),
        RGB( 66,  66,  66),
        RGB( 74,  74,  74),
        RGB( 42,  42,  42),
        RGB( 90,  90,  90),
        RGB( 58,  58,  58),
        RGB( 86,  86,  86),
        RGB( 54,  54,  54),
        RGB(102, 102, 102),
        RGB( 70,  70,  70),
        RGB( 78,  78,  78),
        RGB( 46,  46,  46),
        RGB( 94,  94,  94),
        RGB( 62,  62,  62),
        RGB( 81,  81,  81),
        RGB( 49,  49,  49),
        RGB( 97,  97,  97),
        RGB( 65,  65,  65),
        RGB( 73,  73,  73),
        RGB( 41,  41,  41),
        RGB( 89,  89,  89),
        RGB( 57,  57,  57),
        RGB( 85,  85,  85),
        RGB( 53,  53,  53),
        RGB(101, 101, 101),
        RGB( 69,  69,  69),
        RGB( 77,  77,  77),
        RGB( 45,  45,  45),
        RGB( 93,  93,  93),
        RGB( 61,  61,  61),
        RGB( 83,  83,  83),
        RGB( 51,  51,  51),
        RGB( 99,  99,  99),
        RGB( 67,  67,  67),
        RGB( 75,  75,  75),
        RGB( 43,  43,  43),
        RGB( 91,  91,  91),
        RGB( 59,  59,  59),
        RGB( 87,  87,  87),
        RGB( 55,  55,  55),
        RGB(103, 103, 103),
        RGB( 71,  71,  71),
        RGB( 79,  79,  79),
        RGB( 47,  47,  47),
        RGB( 95,  95,  95),
        RGB( 63,  63,  63),
    };

    // Original upstream palette, retained for reversible color selection.
    static constexpr COLORREF OriginalCushionColors[] = {
        RGB(  0,   0, 255),
        RGB(255,   0,   0),
        RGB(  0, 255,   0),
        RGB(255, 255,   0),
        RGB(  0, 255, 255),
        RGB(255,   0, 255),
        RGB(255, 170,   0),
        RGB(  0,  85, 255),
        RGB(255,   0,  85),
        RGB( 85, 255,   0),
        RGB(170,   0, 255),
        RGB(  0, 255,  85),
        RGB(255,   0, 170),
        RGB(  0, 170, 255),
        RGB(255,  85,   0),
        RGB(  0, 255, 170),
        RGB( 85,   0, 255),
        RGB(255, 255, 255),
    };

    static constexpr int HitTestCellSize = 16;
    const CItem* m_layoutRoot = nullptr;
    CRect m_layoutArea;
    int m_hitTestColumns = 0;
    int m_hitTestRows = 0;
    std::vector<VisibleItem> m_visibleItems;
    std::unordered_map<const CItem*, std::size_t> m_itemToVisibleIndex;
    // Compressed per-cell candidate lists avoid one heap allocation for every
    // spatial bucket while keeping mouse hit-testing bounded to a 16px cell.
    std::vector<std::size_t> m_hitTestCellOffsets;
    std::vector<std::size_t> m_hitTestEntries;

    // Reused for DCs that cannot expose a compatible top-down DIB.
    std::vector<COLORREF> m_bitmapBits;

    Options m_options; // Current options
    double m_lx = 0.0; // Derived parameters
    double m_ly = 0.0;
    double m_lz = 0.0;
};

//
// CTreeMapPreview. A child window, which demonstrates the options
// with an own little demo tree.
//
class CTreeMapPreview final : public CStatic
{
public:
    CTreeMapPreview();
    ~CTreeMapPreview() override;
    void SetOptions(const CTreeMap::Options* options);
    void SetPalette(bool grayscale);

protected:
    void BuildDemoData();

    bool m_grayscale = true;
    CItem* m_root;                  // Demo tree
    CTreeMap m_treeMap;             // Our treemap creator

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
};
