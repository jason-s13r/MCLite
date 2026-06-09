#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <vector>

#include "../hal/Display.h"

namespace mclite {

// Full-screen map view: renders slippy tiles from SD centred on a contact's
// location, with close / zoom-in / center / zoom-out controls overlaid.
// Drag the canvas with one finger to pan; Center snaps back to the contact's
// original lat/lon.
class MapScreen {
public:
    // Open centered on a single contact (telemetry "Map" button).
    void open(double contactLat, double contactLon, const String& contactName);

    // Open the general map: own location + markers for every heard node with GPS.
    // Centers on own location, else the most-recent heard-with-GPS node. No-ops
    // (no open) if there's nothing to show — caller should have checked tiles.
    void openGeneral();

    // Close: restores the previous screen, deletes widgets and the canvas
    // buffer. Safe to call when not open.
    void close();

    bool isOpen() const { return _screen != nullptr; }

private:
    // --- lifecycle ---
    void doOpen();                 // shared tail of open()/openGeneral()
    bool chooseGeneralCenter(double& lat, double& lon);  // own GPS, else a known node location
    void buildMarkers();           // gather known node locations (contacts + heard), deduped
    void buildWidgets();
    void destroyWidgets();
    void pickInitialZoom();

    // --- rendering ---
    void render();
    void renderTiles();
    void drawContactMarker();
    void drawHeardMarkers();       // general mode: type letters for heard nodes w/ GPS
    void drawOwnMarker();
    // lat/lon -> canvas pixel; returns false if well off-canvas. Shared by draw + tap.
    bool markerScreenPos(double lat, double lon, int& px, int& py) const;
    void hitTestMarker(const lv_point_t& p);  // tap → show nearest node's name in _selLabel
    void drawScaleBar();
    void drawCrosshair();
    void updateZoomButtons();
    void updateInfoLabel();

    // --- input ---
    static void closeBtnCb(lv_event_t* e);
    static void zoomInCb(lv_event_t* e);
    static void zoomOutCb(lv_event_t* e);
    static void centerBtnCb(lv_event_t* e);
    static void screenKeyCb(lv_event_t* e);
    static void panPressedCb(lv_event_t* e);
    static void panPressingCb(lv_event_t* e);
    static void panReleasedCb(lv_event_t* e);

    // --- state ---
    lv_obj_t*   _screen       = nullptr;
    lv_obj_t*   _prevScreen   = nullptr;
    lv_obj_t*   _canvas       = nullptr;
    lv_color_t* _cbuf         = nullptr;
    lv_obj_t*   _closeBtn     = nullptr;
    lv_obj_t*   _zoomInBtn    = nullptr;
    lv_obj_t*   _zoomOutBtn   = nullptr;
    lv_obj_t*   _centerBtn    = nullptr;
    lv_obj_t*   _infoLabel    = nullptr;
    lv_obj_t*   _selLabel     = nullptr;   // tapped-marker name (general mode), hidden by default
    lv_group_t* _mapGroup     = nullptr;
    lv_group_t* _prevGroup    = nullptr;

    bool        _general      = false;     // true = general map (heard markers), false = single contact

    // Known node locations for the general map (rebuilt each render). Sources:
    // mesh contacts (fresh telemetry location, else advert GPS) + heard adverts
    // with GPS, deduped by pubkey.
    struct MapMarker { double lat; double lon; uint8_t type; bool isContact; char name[32]; uint8_t key[32]; };
    std::vector<MapMarker> _markers;

    // Tapped marker (highlighted with a ring while its name shows in _selLabel).
    bool    _hasSel = false;
    uint8_t _selKey[32] = {0};

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
    lv_point_t  _panStart{0, 0};   // press point, for tap-vs-pan disambiguation
    uint32_t    _lastRenderMs = 0;

    std::vector<uint8_t> _zooms;  // snapshot from TileLoader
    int      _zoomIdx = 0;
    uint8_t  _zoom    = 0;

    static constexpr int CANVAS_W = Display::width();
    static constexpr int CANVAS_H = Display::height();
};

}  // namespace mclite
