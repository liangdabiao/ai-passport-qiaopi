idf_component_register(
    SRCS "main.c"
         # 应用与页面
         "ddj_app.c"
         "ddj_home.c"
         "ddj_daily.c"
         "ddj_catalog.c"
         "ddj_shelf.c"
         "ddj_settings.c"
         "ddj_ui.c"
         # 纯逻辑：与 ESP-IDF/LVGL 解耦，宿主测试直接编译这些文件
         "ddj_session.c"
         "ddj_progress.c"
         "ddj_chapter.c"
         "ddj_wrap.c"
         "ddj_text.c"
         # 服务
         "ddj_store.c"
         "ddj_sound.c"
         "rtttl_player.c"
    INCLUDE_DIRS "."
    # 本应用只用 BSP 与 NVS。基线 demo 的蓝牙/Wi-Fi/网络组件不再被引用；
    # 基线 demo 的源码仍留在 main/ 里作参考，只是不参与本应用的构建。
    REQUIRES bsp esp_event esp_hw_support esp_timer nvs_flash
)

# 中文字库由 tools/daodejing/gen_font.py 生成到 assets/fonts/，按官方推荐方式作为
# main 组件的额外源文件编译进来。只用这一条路径，不要再把同一个 .c 同时加进
# SRCS，也不要被别的 .c 直接 include。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/ddj_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/ddj_font_24.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/ddj_font_32.c"
)
