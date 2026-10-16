#define PLUG_NAME "SLEWFOLD"
#define PLUG_MFR "MartialSystems"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#define PLUG_UNIQUE_ID 'SmCb'
#define PLUG_MFR_ID 'Mrsy'
#define PLUG_URL_STR "https://github.com/martialsystems/serge_middle"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2026 Martial Systems LLC"
#define PLUG_CLASS_NAME SergeMiddleCombined

#define BUNDLE_NAME "SergeMiddleCombined"
#define BUNDLE_MFR "MartialSystems"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "SergeMiddleCombined"

#define PLUG_CHANNEL_IO "1-1 2-2"

// Same host latency as the PLEAT plugin. The two 63-tap filters delay
// 31 + 31 samples at the 4x rate, 15.5 audio samples. The host is told 16.
#define PLUG_LATENCY 16
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
// 1600 by 960 is slewfold_plate.png, the hand-drawn face. @2x is 3200 by 1920.
// Default fits a 1920 by 1080 screen. The minimum window is 1200 by 720,
// which fits a 1280 by 800 screen. Scale mode keeps the aspect.
#define PLUG_WIDTH 1600
#define PLUG_HEIGHT 960
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 1
#define PLUG_MIN_WIDTH 1200
#define PLUG_MIN_HEIGHT 720
#define PLUG_MAX_WIDTH 2400
#define PLUG_MAX_HEIGHT 1440

#define AUV2_ENTRY SergeMiddleCombined_Entry
#define AUV2_ENTRY_STR "SergeMiddleCombined_Entry"
#define AUV2_FACTORY SergeMiddleCombined_Factory
#define AUV2_VIEW_CLASS SergeMiddleCombined_View
#define AUV2_VIEW_CLASS_STR "SergeMiddleCombined_View"

#define AAX_TYPE_IDS 'SmC1', 'SmC2'
#define AAX_TYPE_IDS_AUDIOSUITE 'SmA1', 'SmA2'
#define AAX_PLUG_MFR_STR "MartialSystems"
#define AAX_PLUG_NAME_STR "SLEWFOLD\nSmCb"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 0

#define VST3_SUBCATEGORY "Fx"

#define CLAP_MANUAL_URL ""
#define CLAP_SUPPORT_URL ""
#define CLAP_DESCRIPTION "SLEWFOLD, PLEAT and slope with patch jacks"
#define CLAP_FEATURES "audio-effect", "distortion"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
