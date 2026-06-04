#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <vector>

#include "../hal/Display.h"

namespace mclite {

// Map view: renders slippy tiles from SD centred on a contact's location,
// with close / zoom-in / center / zoom-out controls overlaid.
// Drag the canvas with one finger to pan; Center snaps back to the contact's
// original lat/lon.
// Now a normal _mainScreen child with create/show/hide lifecycle.
class MapScreen {
public:
    // One-time widget creation (called from UIManager::init)
    void create(lv_obj_t* parent);

    // Show / hide (toggles LV_OBJ_FLAG_HIDDEN)
    void show();
    void hide();

    // Open with contact data — sets lat/lon/name, shows, and renders.
    void open(double contactLat, double contactLon, const String& contactName);

    // Close — hides and fires onBack callback so UIManager restores prev screen.
    void close();

    bool isOpen() const { return _screen != nullptr && !lv_obj_has_flag(_screen, LV_OBJ_FLAG_HIDDEN); }

    // Callback when user presses back / Esc
    void onBack(std::function<void()> cb) { _onBack = cb; }

private:
    // --- lifecycle ---
    void buildWidgets();
    void destroyWidgets();
    void pickInitialZoom();

    // --- rendering ---
    void render();
    void renderTiles();
    void drawContactMarker();
    void drawOwnMarker();
    void drawScaleBar();
    void drawCrosshair();
    void updateZoomButtons();
    void updateInfoLabel();

    // --- input ---
    static void backBtnCb(lv_event_t* e);
    static void zoomInCb(lv_event_t* e);
    static void zoomOutCb(lv_event_t* e);
    static void centerBtnCb(lv_event_t* e);
    static void screenKeyCb(lv_event_t* e);
    static void panPressedCb(lv_event_t* e);
    static void panPressingCb(lv_event_t* e);
    static void panReleasedCb(lv_event_t* e);

    // --- state ---
    lv_obj_t*   _screen       = nullptr;  // parent object on _mainScreen
    lv_obj_t*   _win          = nullptr;  // lv_win widget
    lv_obj_t*   _canvas       = nullptr;
    lv_color_t* _cbuf         = nullptr;
    lv_obj_t*   _backBtn      = nullptr;
    lv_obj_t*   _zoomInBtn    = nullptr;
    lv_obj_t*   _zoomOutBtn   = nullptr;
    lv_obj_t*   _centerBtn    = nullptr;
    lv_obj_t*   _infoLabel    = nullptr;
    lv_obj_t*   _titleLabel   = nullptr;
    lv_group_t* _mapGroup     = nullptr;
    lv_group_t* _prevGroup    = nullptr;

    std::function<void()> _onBack;
    lv_obj_t* _parent = nullptr;

    // Original contact location — set once in open(), used by drawContactMarker
    // and the Center button. Constant for the lifetime of the screen.
    double   _contactLat = 0.0;
    double   _contactLon = 0.0;
    String   _contactName;

    // Current viewport centre — starts at the contact location, mutated by
    // drag-to-pan and reset by the Center button.
    double   _centerLat = 0.0;
    double   _centerLon = 0.0;

    // Pan-gesture state.
    bool        _panActive    = false;
    lv_point_t  _panLast{0, 0};
    uint32_t    _lastRenderMs = 0;

    std::vector<uint8_t> _zooms;  // snapshot from TileLoader
    int      _zoomIdx = 0;
    uint8_t  _zoom    = 0;

    int _canvasW = 0;
    int _canvasH = 0;
};

}  // namespace mclite
