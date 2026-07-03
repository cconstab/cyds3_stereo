# Post-build: produce firmware-merged.bin (bootloader + partition table + boot_app0 + app)
# suitable for flashing a factory-fresh board at offset 0x0 (esptool or ESP Web Tools).
Import("env")


def merge_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    merged = f"{build_dir}/firmware-merged.bin"

    cmd = [
        '"$PYTHONEXE"',
        '"$OBJCOPY"',  # esptool.py in the espressif toolchain
        "--chip", "esp32s3",
        "merge_bin",
        "-o", f'"{merged}"',
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "16MB",
    ]
    # bootloader, partition table, boot_app0 (offset, path) pairs
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        cmd += [offset, f'"{env.subst(image)}"']
    cmd += [env.subst("$ESP32_APP_OFFSET"), f'"{target[0].get_abspath()}"']

    env.Execute(" ".join(cmd))
    print(f"Merged image: {merged}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
