# Universal early-load snippet for any Android game APK
#
# 1. Find the launcher activity from AndroidManifest.xml
#    (look for android.intent.action.MAIN + LAUNCHER)
# 2. Open its .smali file under smali*/...
# 3. Locate:
#        .method protected onCreate(Landroid/os/Bundle;)V
#    (or .method public onCreate...)
# 4. Immediately after the .locals / .registers line, insert the two lines below.
#    Pick any free register if v0 is already live.

    const-string v0, "entpass"
    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

# Example with registers already declared:
#
# .method protected onCreate(Landroid/os/Bundle;)V
#     .locals 3
#     const-string v0, "entpass"
#     invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
#     … original onCreate body …
#
# Alternative locations that also work early:
#   - Application.onCreate
#   - static constructor of the main Activity
#   - any class that is loaded before the Platform SDK initialises
