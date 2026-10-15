# Universal Meta / Oculus entitlement force for any game

Forces the client-side Meta / Oculus Platform entitlement check to succeed so the
log line

    Meta app entitlement check failed: user does not have app in library

(or any equivalent) never aborts launch.

Works on any Android APK that uses the Oculus/Meta Platform SDK
(`ovr_Message_IsError` / `ovr_Message_GetType`).  Gorilla Tag, other VR titles,
sideloaded SPM builds, and non-VR Meta-integrated games are all in scope.

This is a pure client-side force.  Server-side bans, anti-cheat, and remote
entitlement remain untouched.

## Layout

```
entpass_universal/
├── src/libentpass.cpp          # native source (universal)
├── scripts/build_so.sh         # builds arm64-v8a + armeabi-v7a
├── smali/load_entpass.smali    # exact loadLibrary snippet
├── README.md                   # this file
└── out/                        # created by the build script
    ├── arm64-v8a/libentpass.so
    └── armeabi-v7a/libentpass.so
```

## Prerequisites (one-time)

- Android NDK r23 or newer
- apktool, zipalign, apksigner (Android build-tools)
- A keystore for signing (debug keystore is fine for personal use)

```bash
# Debian / Ubuntu example
sudo apt install apktool zipalign apksigner

# NDK – download from https://developer.android.com/ndk/downloads
export ANDROID_NDK_HOME=/path/to/ndk
```

## 1. Build the library

```bash
cd entpass_universal
chmod +x scripts/build_so.sh
./scripts/build_so.sh
```

Output:

```
out/arm64-v8a/libentpass.so
out/armeabi-v7a/libentpass.so
```

Most modern Quest / Android titles are arm64-v8a only.  Include both ABIs if
the original APK ships both.

## 2. Inject into any game APK

Replace `GAME.apk` with the path to the base APK you want to patch.

### 2.1 Unpack

```bash
apktool d GAME.apk -o game_src
```

### 2.2 Drop the native library

```bash
# create the ABI folder(s) that match the original APK
mkdir -p game_src/lib/arm64-v8a
cp out/arm64-v8a/libentpass.so game_src/lib/arm64-v8a/

# only if the original APK also contains armeabi-v7a:
mkdir -p game_src/lib/armeabi-v7a
cp out/armeabi-v7a/libentpass.so game_src/lib/armeabi-v7a/
```

### 2.3 Force early load (smali)

1. Open `game_src/AndroidManifest.xml`.
2. Find the activity that has both
   `android.intent.action.MAIN` and `android.intent.category.LAUNCHER`.
3. Open the corresponding `.smali` file under `smali*/…`.
   Common names:
   - `com/unity3d/player/UnityPlayerActivity.smali`
   - `com/unity3d/player/UnityPlayerGameActivity.smali`
   - package-specific launcher activity
4. Locate the method:

   ```
   .method protected onCreate(Landroid/os/Bundle;)V
   ```

5. Immediately after the `.locals N` / `.registers N` line, insert:

   ```
       const-string v0, "entpass"
       invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
   ```

   (Exact lines are in `smali/load_entpass.smali`.)

   If register `v0` is already live, use any free register (`v1`, `v2`, …).

Alternative early-load points that also work:
- `Application.onCreate`
- static constructor of the main activity
- any class that is guaranteed to load before the Platform SDK initialises

### 2.4 Rebuild

```bash
apktool b game_src -o game_entpass.apk
```

### 2.5 Align + sign

```bash
zipalign -f 4 game_entpass.apk game_entpass_aligned.apk

apksigner sign --ks your.keystore --ks-key-alias youralias \
  --out GAME-entpass.apk game_entpass_aligned.apk
```

(Use a debug keystore if you do not have a release one.)

### 2.6 Install

Keep existing OBB / data folders.  Only the base APK is replaced.

```bash
adb install -r GAME-entpass.apk
```

## 3. Verify

```bash
adb logcat -s entpass Unity MetaAuthenticator Oculus Platform
```

Expected lines:

```
I/entpass: libentpass constructor (universal)
I/entpass: JNI_OnLoad
I/entpass: symbols resolved – entitlement path forced clean
```

or, if the platform library loads later:

```
I/entpass: found already-loaded libovrplatformloader.so
I/entpass: symbols resolved – entitlement path forced clean
```

The original “user does not have app in library” (or equivalent) line should
no longer appear.

## Notes

- OBB / il2cpp / asset packs stay exactly where they are.  Only the base APK is
  touched.
- The library retries for ~6 s if the platform loader is not present at
  constructor time (common on some Unity titles).
- If a particular title still fails after symbols resolve, the message-type
  constant may differ.  Dump the platform `.so` and update
  `OVR_MESSAGE_ENTITLEMENT` in `src/libentpass.cpp`, then rebuild.
- For instruction-level trampolines (rare) replace the note in the source with
  Dobby / And64InlineHook; the current implementation is sufficient for the
  vast majority of sideloaded builds because the managed callback only inspects
  the IsError return value.
- This is client-side only.  Server entitlement, bans, and anti-cheat are
  unaffected.

## Troubleshooting

| Symptom | Action |
|---------|--------|
| `platform loader not present yet` forever | Confirm the APK actually links a libovrplatform*.so; check `lib/` folders after unpack |
| symbols resolved but check still fails | Update `OVR_MESSAGE_ENTITLEMENT` from a dump of the platform library |
| loadLibrary fails at runtime | ABI mismatch – copy the correct `out/<abi>/` folder |
| apktool rebuild errors | Use the same apktool version that decoded cleanly; fix any framework jars if needed |
