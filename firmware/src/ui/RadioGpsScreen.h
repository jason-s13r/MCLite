#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <vector>

namespace mclite {

struct RadioPreset {
    String title;
    String description;
    float  frequency;
    uint8_t spreadingFactor;
    float  bandwidth;
    uint8_t codingRate;
};

struct EditRowCtx {
    const char* label;
    const char* field;
    String      value;
};

class RadioGpsScreen {
public:
    void create(lv_obj_t* parent);
    void show();
    void hide();

    lv_obj_t* obj() { return _screen; }

private:
    lv_obj_t* _screen   = nullptr;
    lv_obj_t* _content  = nullptr;

    // Edit modal state
    lv_obj_t* _editModal     = nullptr;
    lv_obj_t* _editTextarea  = nullptr;
    String    _editField;   // "freq", "sf", "bw", "cr", "txp", "scope", "path"

    // Preset modal state
    lv_obj_t* _presetModal   = nullptr;
    lv_obj_t* _presetList    = nullptr;
    std::vector<RadioPreset> _presets;

    // Path hash choice modal
    lv_obj_t* _pathModal     = nullptr;

    void showEditModal(const char* title, const String& currentValue, const char* field);
    void hideEditModal();
    void showPresetModal();
    void hidePresetModal();
    void showPathModal();
    void hidePathModal();
    void loadPresets();
    void applyPreset(const RadioPreset& preset);
    void saveRadioAndReboot();

    static void backBtnCb(lv_event_t* e);
    static void offgridToggleCb(lv_event_t* e);
    static void gpsToggleCb(lv_event_t* e);
    static void locationAdvertToggleCb(lv_event_t* e);
    static void locationPrecisionChangedCb(lv_event_t* e);
    static const char* precisionLabel(uint8_t precision);

    static void editRowCb(lv_event_t* e);
    static void editConfirmCb(lv_event_t* e);
    static void editCancelCb(lv_event_t* e);
    static void presetBtnCb(lv_event_t* e);
    static void presetRowCb(lv_event_t* e);
    static void presetCloseCb(lv_event_t* e);
};

}  // namespace mclite
