#define PLUG_NAME "SergeMiddle"
#define PLUG_MFR "MartialSystems"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#define PLUG_UNIQUE_ID 'Smid'
#define PLUG_MFR_ID 'Mrsy'
#define PLUG_URL_STR "https://github.com/martialsystems/serge_middle"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2026 Martial Systems LLC"
#define PLUG_CLASS_NAME SergeMiddle

#define BUNDLE_NAME "SergeMiddle"
#define BUNDLE_MFR "MartialSystems"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "SergeMiddle"

#define PLUG_CHANNEL_IO "1-1 2-2"

// Group delay of the two 63-tap filters is 31 + 31 samples at the 4x rate,
// 15.5 audio samples. The host is told 16. See SergeMiddle.cpp.
#define PLUG_LATENCY 16
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 300
#define PLUG_HEIGHT 300
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY SergeMiddle_Entry
#define AUV2_ENTRY_STR "SergeMiddle_Entry"
#define AUV2_FACTORY SergeMiddle_Factory
#define AUV2_VIEW_CLASS SergeMiddle_View
#define AUV2_VIEW_CLASS_STR "SergeMiddle_View"

#define AAX_TYPE_IDS 'SMD1', 'SMD2'
#define AAX_TYPE_IDS_AUDIOSUITE 'SMA1', 'SMA2'
#define AAX_PLUG_MFR_STR "MartialSystems"
#define AAX_PLUG_NAME_STR "SergeMiddle\nSMID"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 0

#define VST3_SUBCATEGORY "Fx|Distortion"

#define CLAP_MANUAL_URL ""
#define CLAP_SUPPORT_URL ""
#define CLAP_DESCRIPTION "Serge Wave Multipliers middle section"
#define CLAP_FEATURES "audio-effect", "distortion"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
