Import("env")
import os
import shutil

# Force sdkconfig regeneration to pick up our Bluetooth settings
# PlatformIO/Arduino sometimes doesn't respect sdkconfig.defaults properly

project_dir = env.get("PROJECT_DIR")
build_dir = env.get("BUILD_DIR")

# Remove cached sdkconfig to force regeneration
sdkconfig_path = os.path.join(build_dir, "sdkconfig")
if os.path.exists(sdkconfig_path):
    print(f"Removing cached sdkconfig: {sdkconfig_path}")
    os.remove(sdkconfig_path)

# Copy our sdkconfig.defaults to the build directory as sdkconfig
src_sdkconfig = os.path.join(project_dir, "sdkconfig.defaults")
if os.path.exists(src_sdkconfig):
    print(f"Copying {src_sdkconfig} to {sdkconfig_path}")
    shutil.copy(src_sdkconfig, sdkconfig_path)
