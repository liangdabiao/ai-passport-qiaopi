idf_component_register(
    SRCS "main.c"
         # 应用与页面
         "szj_app.c"
         "szj_home.c"
         "szj_lesson.c"
         "szj_card.c"
         "szj_settings.c"
         "szj_ui.c"
         # 纯逻辑:与 ESP-IDF/LVGL 解耦,宿主测试直接编译这些文件
         "szj_session.c"
         "szj_progress.c"
         "szj_quiz.c"
         "szj_text.c"
         "szj_text_util.c"
         # 服务
         "szj_store.c"
         "szj_sound.c"
         "rtttl_player.c"
    INCLUDE_DIRS "."
    # 本应用只用 BSP 与 NVS。基线 demo 的蓝牙/Wi-Fi/网络组件不再被引用;
    # 基线 demo 的源码仍留在 main/ 里作参考,只是不参与本应用的构建。
    REQUIRES bsp esp_event esp_hw_support esp_timer nvs_flash
)

# 中文字库由 tools/sanzijing/gen_font.py 生成到 assets/fonts/,按官方推荐方式作为
# main 组件的额外源文件编译进来(见 docs/development/engineering/lvgl-chinese-fonts.md
# 的 "Compile and declare the generated source")。只用这一条路径,不要再把同一个
# .c 同时加进 SRCS,也不要被别的 .c 直接 include。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/szj_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/szj_font_24.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/szj_font_32.c"
)
