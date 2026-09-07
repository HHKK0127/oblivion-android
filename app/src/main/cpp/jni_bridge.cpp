#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include "engine/renderer.h"
#include "engine/imperial_weave.h"
#include "vegetation/speed_tree_manager.h"
#include "video/bink_video_player.h"
#include "video/video_decoder_jni.h"

#define LOG_TAG "JNI_Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static Renderer* g_renderer = nullptr;
AAssetManager* g_assetManager = nullptr;

extern "C" {
    void jni_audio_set_asset_manager(AAssetManager* mgr);
    void jni_audio_set_java_vm(JavaVM* vm);
    void jni_audio_set_main_activity(jobject activity);
}

// Initialize engine and return handle to Java
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_oblivion_GameRenderer_nativeInitEngine(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("=== nativeInitEngine called ===");

    if (g_renderer != nullptr) {
        LOGI("Renderer already existed when new OpenGL context was created (app resume).");
        LOGI("Cleaning up and deleting old Renderer to ensure all OpenGL resources are recreated on the new context...");
        g_renderer->cleanup();
        delete g_renderer;
        g_renderer = nullptr;
    }

    LOGI("Creating new Renderer instance...");
    try {
        LOGI("STEP 1: Allocating Renderer object...");
        g_renderer = new Renderer();
        LOGI("STEP 1: SUCCESS - Renderer object allocated");

        LOGI("STEP 2: Calling Renderer::init(1920, 1080)...");
        bool initResult = g_renderer->init(1920, 1080);  // Default size, will be updated by onSurfaceChanged

        LOGI("STEP 2: init() returned %d", initResult ? 1 : 0);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "===== INIT RESULT: %s =====",
                            initResult ? "SUCCESS" : "FAILED");

        if (!initResult) {
            LOGE("CRITICAL: Renderer::init() returned false - initialization failed");
            LOGE("Deleting Renderer instance...");
            delete g_renderer;
            g_renderer = nullptr;
            LOGE("Renderer deleted, returning 0 to Java");
            return 0;
        }

        LOGI("SUCCESS: Renderer initialized successfully");
        jlong handle = reinterpret_cast<jlong>(g_renderer);
        LOGI("Returning handle to Java: %p", g_renderer);
        return handle;
    } catch (const std::exception& e) {
        LOGE("EXCEPTION in nativeInitEngine: %s", e.what());
        LOGE("Exception type details: checking std::exception");
        if (g_renderer) {
            delete g_renderer;
            g_renderer = nullptr;
        }
        return 0;
    } catch (...) {
        LOGE("UNKNOWN EXCEPTION in nativeInitEngine");
        if (g_renderer) {
            delete g_renderer;
            g_renderer = nullptr;
        }
        return 0;
    }
}

// Initialize audio bridge with asset manager & JavaVM (Phase 8+)
extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeInitAudioBridge(
        JNIEnv* env,
        [[maybe_unused]] jclass clazz,
        jobject assetManager,
        jobject mainActivity) {
    LOGI("nativeInitAudioBridge called");

    if (!assetManager) {
        LOGE("AssetManager is null");
        return;
    }

    // Get AAssetManager and set to global variable (used by TextRenderer and Audio)
    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
    if (!mgr) {
        LOGE("Failed to get AAssetManager from Java");
        return;
    }
    g_assetManager = mgr;
    LOGI("g_assetManager set successfully: %p", g_assetManager);

#ifdef AUDIO_SYSTEM_ENABLED
    jni_audio_set_asset_manager(mgr);
    LOGD("AAssetManager set for audio system");

    // Get and set JavaVM
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK) {
        LOGE("Failed to get JavaVM");
        return;
    }
    jni_audio_set_java_vm(vm);
    LOGD("JavaVM set");

    // Set MainActivity instance
    if (mainActivity) {
        // Create global reference for long-term storage
        jobject globalMainActivity = env->NewGlobalRef(mainActivity);
        if (!globalMainActivity) {
            LOGE("Failed to create global reference for MainActivity");
            return;
        }
        jni_audio_set_main_activity(globalMainActivity);
        LOGD("MainActivity global reference set");
    } else {
        LOGE("MainActivity object is null");
        return;
    }
#else
    LOGD("Audio system disabled, skipping audio bridge setup");
#endif

    LOGI("Asset manager bridge initialized successfully");
}

// Set viewport with handle parameter
extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeSetViewport(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jlong handle,
        jint width,
        jint height) {
    LOGD("nativeSetViewport called: %d x %d", width, height);

    Renderer* renderer = reinterpret_cast<Renderer*>(handle);
    if (renderer) {
        renderer->resize(width, height);
    }
}

// Set view (physical) size for touch coordinate conversion
extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeSetViewSize(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jlong handle,
        jfloat width,
        jfloat height) {
    LOGD("nativeSetViewSize called: %.1f x %.1f", width, height);
    Renderer* renderer = reinterpret_cast<Renderer*>(handle);
    if (renderer) {
        renderer->setViewSize(width, height);
    }
}

// Render frame with handle parameter
extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeRenderFrame(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jlong handle) {
    Renderer* renderer = reinterpret_cast<Renderer*>(handle);
    if (renderer) {
        renderer->render(0.0167f);  // 60 FPS default (1/60 sec)
    } else {
        LOGD("WARNING: nativeRenderFrame called with null renderer handle");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeCleanup(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("nativeCleanup called");

    if (g_renderer) {
        g_renderer->cleanup();
        delete g_renderer;
        g_renderer = nullptr;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeSetLanguage(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jint language) {
    LOGD("nativeSetLanguage called: %d", language);

    if (g_renderer) {
        auto locMgr = g_renderer->getLocalizationManager();
        if (locMgr) {
            locMgr->setLanguage(static_cast<Language>(language));
        }
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_oblivion_GameRenderer_nativeGetLanguage(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    if (g_renderer) {
        auto locMgr = g_renderer->getLocalizationManager();
        if (locMgr) {
            return static_cast<jint>(locMgr->getLanguage());
        }
    }
    return 0;  // ENGLISH
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_oblivion_GameRenderer_nativeGetString(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring key) {
    if (!g_renderer) {
        return env->NewStringUTF("");
    }

    auto locMgr = g_renderer->getLocalizationManager();
    if (!locMgr) {
        return env->NewStringUTF("");
    }

    const char* keyStr = env->GetStringUTFChars(key, nullptr);
    std::string result = locMgr->getString(keyStr);
    env->ReleaseStringUTFChars(key, keyStr);

    return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeOnTouchEvent(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jlong handle,
        jint pointerId,
        jfloat x,
        jfloat y,
        jint action) {
    __android_log_print(ANDROID_LOG_DEBUG, "JNIBridge", "nativeOnTouchEvent called: handle=%ld, pointerId=%d, x=%.1f, y=%.1f, action=%d", (long)handle, pointerId, x, y, action);
    Renderer* renderer = reinterpret_cast<Renderer*>(handle);
    if (renderer) {
        __android_log_print(ANDROID_LOG_DEBUG, "JNIBridge", "Calling renderer->onTouchEvent...");
        renderer->onTouchEvent(pointerId, x, y, action);
        __android_log_print(ANDROID_LOG_DEBUG, "JNIBridge", "renderer->onTouchEvent returned");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "JNIBridge", "nativeOnTouchEvent: renderer is NULL!");
    }
}

// Set BSA data path from Java (e.g., external storage path)
extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeSetDataPath(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring dataPath) {
    if (!g_renderer) {
        LOGE("nativeSetDataPath called but renderer is null");
        return;
    }

    AssetManager* am = g_renderer->getAssetManager();
    if (!am) {
        LOGE("nativeSetDataPath called but AssetManager is null");
        return;
    }

    const char* pathStr = env->GetStringUTFChars(dataPath, nullptr);
    am->setDataPath(pathStr);
    LOGI("BSA data path set to: %s", pathStr);
    env->ReleaseStringUTFChars(dataPath, pathStr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeOnKeyPress(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jint key) {
    if (g_renderer && g_renderer->getTitleScreen()) {
        g_renderer->getTitleScreen()->onKeyPress(key);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeTitleScreenActive(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    if (g_renderer) {
        return g_renderer->isTitleScreenActive() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeSetTargetFPS(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jint fps) {
    LOGD("nativeSetTargetFPS called: %d", fps);

    if (g_renderer) {
        g_renderer->setTargetFPS(static_cast<int>(fps));
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_oblivion_GameRenderer_nativeGetTargetFPS(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    if (g_renderer) {
        return static_cast<jint>(g_renderer->getTargetFPS());
    }
    return 60;  // Default
}

// ============================================
// Phase 30 Step 13: Integration Test Runner
// ============================================
#include "tests/phase30_integration_test.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_oblivion_GameRenderer_nativeRunPhase30Test(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring assetPath) {
    const char* path = env->GetStringUTFChars(assetPath, nullptr);
    std::string basePathStr(path);
    env->ReleaseStringUTFChars(assetPath, path);

    LOGI("=== Phase 30 Integration Test START ===");
    LOGI("Asset path: %s", basePathStr.c_str());

    Phase30IntegrationTest test;
    bool allPassed = test.runAllTests(basePathStr);

    std::string summary = test.getSummary();
    LOGI("=== Phase 30 Integration Test END: %s ===",
         allPassed ? "ALL PASSED" : "SOME FAILED");

    return env->NewStringUTF(summary.c_str());
}

// Phase 45: Unit Test Runner
// ============================================
#include "tests/phase45_unit_tests.h"

// ============================================
// Phase 50: Distant LOD System
// ============================================
#include "world/distant_lod/distant_lod_manager.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeInitDistantLod(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jlong worldManagerHandle) {
    LOGI("nativeInitDistantLod called");

    WorldManager* worldMgr = reinterpret_cast<WorldManager*>(worldManagerHandle);
    if (!worldMgr) {
        LOGE("nativeInitDistantLod: WorldManager handle is null");
        return JNI_FALSE;
    }

    DistantLodManager& dlod = DistantLodManager::instance();
    bool result = dlod.initialize(worldMgr, nullptr);

    if (result) {
        // Register with ImperialWeave
        weave::ImperialWeave::instance().getLocator().registerService(&dlod);
        LOGI("DistantLodManager initialized and registered with ImperialWeave");
    } else {
        LOGE("DistantLodManager initialization failed");
    }

    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_oblivion_GameRenderer_nativeRunPhase45Test(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("=== Phase 45 Unit Test START ===");

    Phase45UnitTests test;
    bool allPassed = test.runAllTests();

    std::string summary = test.getSummary();
    LOGI("=== Phase 45 Unit Test END: %s ===",
         allPassed ? "ALL PASSED" : "SOME FAILED");

    return env->NewStringUTF(summary.c_str());
}

// ============================================
// Phase 48: Game Loop Integration Test Runner
// ============================================
#include "tests/phase48_integration_test.h"
#include "tests/phase48_stress_test.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_oblivion_GameRenderer_nativeRunPhase48Tests(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("=== Phase 48 Integration Test START ===");

    Phase48IntegrationTest test;
    bool allPassed = test.runAllTests();

    std::string summary = test.getSummary();
    LOGI("=== Phase 48 Integration Test END: %s ===",
         allPassed ? "ALL PASSED" : "SOME FAILED");

    return env->NewStringUTF(summary.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_oblivion_GameRenderer_nativeRunPhase48StressTests(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("=== Phase 48 Stress Test START ===");

    Phase48StressTest test;
    bool allPassed = test.runAllTests();

    std::string summary = test.getSummary();
    LOGI("=== Phase 48 Stress Test END: %s ===",
         allPassed ? "ALL PASSED" : "SOME FAILED");

    return env->NewStringUTF(summary.c_str());
}

// ============================================
// ============================================
// Phase 51: SpeedTree Vegetation System
// ============================================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeInitSpeedTree(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jlong handle) {
    LOGI("=== nativeInitSpeedTree called ===");

    Renderer* renderer = reinterpret_cast<Renderer*>(handle);
    if (!renderer) {
        LOGE("nativeInitSpeedTree: null renderer handle");
        return JNI_FALSE;
    }

    auto& speedTree = vegetation::SpeedTreeManager::instance();
    bool result = speedTree.initialize(renderer);

    if (result) {
        // Register default tree types
        vegetation::TreeType oakType;
        oakType.typeId = 1;
        oakType.meshPath = "meshes/trees/oak01.nif";
        oakType.texturePath = "textures/trees/oak_bark.dds";
        oakType.billboardTexturePath = "textures/trees/oak_billboard.dds";
        oakType.minHeight = 5.0f;
        oakType.maxHeight = 12.0f;
        oakType.billboardWidth = 5.0f;
        oakType.billboardHeight = 10.0f;
        speedTree.registerTreeType(1, oakType);

        vegetation::TreeType pineType;
        pineType.typeId = 2;
        pineType.meshPath = "meshes/trees/pine01.nif";
        pineType.texturePath = "textures/trees/pine_bark.dds";
        pineType.billboardTexturePath = "textures/trees/pine_billboard.dds";
        pineType.minHeight = 8.0f;
        pineType.maxHeight = 18.0f;
        pineType.billboardWidth = 4.0f;
        pineType.billboardHeight = 14.0f;
        speedTree.registerTreeType(2, pineType);

        vegetation::TreeType mapleType;
        mapleType.typeId = 3;
        mapleType.meshPath = "meshes/trees/maple01.nif";
        mapleType.texturePath = "textures/trees/maple_bark.dds";
        mapleType.billboardTexturePath = "textures/trees/maple_billboard.dds";
        mapleType.minHeight = 4.0f;
        mapleType.maxHeight = 10.0f;
        mapleType.billboardWidth = 6.0f;
        mapleType.billboardHeight = 9.0f;
        speedTree.registerTreeType(3, mapleType);

        LOGI("SpeedTree initialized with %lu tree types",
             (unsigned long)speedTree.getTypeCount());
    } else {
        LOGE("SpeedTree initialization failed");
    }

    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_oblivion_GameRenderer_nativeGetSpeedTreeCount(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    return static_cast<jint>(vegetation::SpeedTreeManager::instance().getTreeCount());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_oblivion_GameRenderer_nativeGetSpeedTreeVisibleCount(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    return static_cast<jint>(vegetation::SpeedTreeManager::instance().getVisibleCount());
}

// ============================================
// Phase 52: FaceGen System Initialization
// ============================================
#include "character/face_gen_morpher.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeInitFaceGen(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("=== nativeInitFaceGen called ===");

    auto& faceGen = facegen::FaceGenMorpher::instance();
    bool result = faceGen.initialize(nullptr, nullptr);

    if (result) {
        LOGI("FaceGenMorpher initialized successfully");
        LOGI("Base mesh vertices: %lu", static_cast<unsigned long>(faceGen.getBaseMeshVertexCount()));
        LOGI("Morph targets: %lu", static_cast<unsigned long>(faceGen.getMorphTargetCount()));
    } else {
        LOGE("FaceGenMorpher initialization failed");
    }

    return result ? JNI_TRUE : JNI_FALSE;
}

// ============================================
// Phase 53: Bink Video System
// ============================================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeInitBinkVideo(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jobject surface) {
    LOGI("=== nativeInitBinkVideo called ===");

    if (!surface) {
        LOGE("nativeInitBinkVideo: surface is null");
        return JNI_FALSE;
    }

    // Get ANativeWindow from Surface
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("nativeInitBinkVideo: failed to get ANativeWindow");
        return JNI_FALSE;
    }

    // Initialize BinkVideoPlayer singleton
    auto& player = oblivion::video::BinkVideoPlayer::instance();
    bool result = player.initialize(window);

    // Release ANativeWindow ref regardless of result (ANativeWindow_fromSurface increments refcount)
    ANativeWindow_release(window);

    if (result) {
        // Initialize JNI references for VideoDecoderJNI
        oblivion::video::VideoDecoderJNI::initJNI(env);
        oblivion::video::registerVideoDecoderNatives(env);

        LOGI("BinkVideoPlayer initialized successfully");
    } else {
        LOGE("BinkVideoPlayer initialization failed");
    }

    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeGenerateNpcFace(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jint npcId) {
    LOGD("nativeGenerateNpcFace called for NPC %d", npcId);

    auto& faceGen = facegen::FaceGenMorpher::instance();
    if (!faceGen.isInitialized()) {
        LOGE("FaceGenMorpher not initialized");
        return JNI_FALSE;
    }

    // Generate face from ESM data if available
    bool result = faceGen.generateFaceFromESM(static_cast<uint32_t>(npcId));
    if (!result) {
        // Generate with default shape if no ESM data
        facegen::FaceShape defaultShape;
        result = faceGen.generateMorphedMesh(static_cast<uint32_t>(npcId), defaultShape);
    }

    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativePlayVideo(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring clipId,
        jboolean loop) {
    if (!clipId) {
        LOGE("nativePlayVideo: clipId is null");
        return JNI_FALSE;
    }

    const char* clipIdStr = env->GetStringUTFChars(clipId, nullptr);
    std::string clipIdCpp(clipIdStr);
    env->ReleaseStringUTFChars(clipId, clipIdStr);

    auto& player = oblivion::video::BinkVideoPlayer::instance();
    bool result = player.play(clipIdCpp, loop == JNI_TRUE);

    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeLoadFaceGenData(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jint npcId,
        jbyteArray fggsData,
        jbyteArray fggaData,
        jbyteArray fgtsData) {
    LOGD("nativeLoadFaceGenData called for NPC %d", npcId);

    auto& faceGen = facegen::FaceGenMorpher::instance();
    if (!faceGen.isInitialized()) {
        LOGE("FaceGenMorpher not initialized");
        return JNI_FALSE;
    }

    // Get byte arrays from Java
    jbyte* fggs = fggsData ? env->GetByteArrayElements(fggsData, nullptr) : nullptr;
    jbyte* fgga = fggaData ? env->GetByteArrayElements(fggaData, nullptr) : nullptr;
    jbyte* fgts = fgtsData ? env->GetByteArrayElements(fgtsData, nullptr) : nullptr;

    jsize fggsLen = fggsData ? env->GetArrayLength(fggsData) : 0;
    jsize fggaLen = fggaData ? env->GetArrayLength(fggaData) : 0;
    jsize fgtsLen = fgtsData ? env->GetArrayLength(fgtsData) : 0;

    // Parse FaceGen record
    facegen::FaceGenRecord record = facegen::FaceGenParser::parseFromSubrecords(
        static_cast<uint32_t>(npcId),
        reinterpret_cast<const uint8_t*>(fggs), static_cast<size_t>(fggsLen),
        reinterpret_cast<const uint8_t*>(fgga), static_cast<size_t>(fggaLen),
        reinterpret_cast<const uint8_t*>(fgts), static_cast<size_t>(fgtsLen)
    );

    // Release byte arrays
    if (fggs) env->ReleaseByteArrayElements(fggsData, fggs, JNI_ABORT);
    if (fgga) env->ReleaseByteArrayElements(fggaData, fgga, JNI_ABORT);
    if (fgts) env->ReleaseByteArrayElements(fgtsData, fgts, JNI_ABORT);

    // Load into FaceGenMorpher
    bool result = faceGen.loadFaceGenData(static_cast<uint32_t>(npcId), record);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeStopVideo(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& player = oblivion::video::BinkVideoPlayer::instance();
    player.stop();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativePauseVideo(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& player = oblivion::video::BinkVideoPlayer::instance();
    player.pause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeResumeVideo(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& player = oblivion::video::BinkVideoPlayer::instance();
    player.resume();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeIsVideoPlaying(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& player = oblivion::video::BinkVideoPlayer::instance();
    return player.isPlaying() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeSetVideoVolume(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jfloat volume) {
    auto& player = oblivion::video::BinkVideoPlayer::instance();
    player.setVolume(volume);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeShutdownBinkVideo(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("nativeShutdownBinkVideo called");
    auto& player = oblivion::video::BinkVideoPlayer::instance();
    player.shutdown();
}

// ============================================
// Debug System Toggle Methods
// ============================================

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeToggleDebugConsole(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("nativeToggleDebugConsole called");
    if (g_renderer) {
        g_renderer->toggleGameConsole();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeToggleNpcDebug(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("nativeToggleNpcDebug called");
    if (g_renderer) {
        g_renderer->toggleNpcDebugVisualizer();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeToggleWorldDebug(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("nativeToggleWorldDebug called");
    if (g_renderer) {
        g_renderer->toggleWorldDebugInfo();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeTogglePerfGraph(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("nativeTogglePerfGraph called");
    if (g_renderer) {
        g_renderer->togglePerformanceGraph();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeToggleAllDebug(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("nativeToggleAllDebug called");
    if (g_renderer) {
        g_renderer->toggleAllDebugSystems();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oblivion_GameRenderer_nativeToggleDebugMenu(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    LOGI("nativeToggleDebugMenu called");
    if (g_renderer) {
        g_renderer->toggleDebugMenu();
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeIsDebugMenuVisible(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    if (g_renderer) {
        return g_renderer->isDebugMenuVisible();
    }
    return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_oblivion_GameRenderer_nativeIsExitRequested(
        [[maybe_unused]] JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    if (g_renderer) {
        return g_renderer->isExitRequested() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}
