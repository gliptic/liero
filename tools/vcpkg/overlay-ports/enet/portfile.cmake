vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO zpl-c/enet
    REF "v${VERSION}"
    SHA512 9f84279abce488d569db90ed64bb88a8c4daf0879771c6229a37cd168320b8f7ca8160a00a5e71f573cccd74c45934a4b1eacb889836003883c8f79c433012ab
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DENET_STATIC=ON
        -DENET_SHARED=OFF
        -DENET_TEST=OFF
)

vcpkg_cmake_build()
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/enet)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
