idf_component_register(
    SRCS "main.c"
         # 应用与页面
         "qpq_app.c"
         "qpq_page_title.c"
         "qpq_page_ask.c"
         "qpq_page_reveal.c"
         "qpq_page_summary.c"
         "qpq_ui.c"
         # 纯逻辑：与 ESP-IDF/LVGL 解耦，宿主测试直接编译这些文件
         "qpq_session.c"
         "qpq_progress.c"
         "qpq_content.c"
         "qpq_wrap.c"
         "qpq_adpcm.c"
         "qpq_audio_index.c"
         "qpq_volume.c"
         "qpq_text.c"
         # 服务
         "qpq_store.c"
         "qpq_player.c"
         # blob 的链接器入口（只参与固件构建，见 qpq_audio_blob.c 的说明）
         "qpq_audio_blob.c"
    INCLUDE_DIRS "."
    # 本应用只用 BSP、NVS 与定时器。基线 demo 的蓝牙/Wi-Fi/网络组件不再被引用；
    # 基线 demo 的源码仍留在 main/ 里作参考，只是不参与本应用的构建。
    #
    # 这里刻意**没有** rtttl_player.c：I2S 出口只能有一个写入者，而本应用的
    # qpq_player 同时负责语音、音乐与音效。两个任务各写各的，结果是两股 PCM
    # 交错插进同一个 DMA 缓冲 —— 听感像噪声，而且很难查。
    REQUIRES bsp esp_event esp_hw_support esp_timer nvs_flash
)

# 中文字库由 tools/qiaopi/gen_font.py 生成到 assets/fonts/，按官方推荐方式作为
# main 组件的额外源文件编译进来。只用这一条路径，不要再把同一个 .c 同时加进
# SRCS，也不要被别的 .c 直接 include。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/qpq_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/qpq_font_24.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/qpq_font_32.c"
)

# 方言配音与背景音乐：约 5 MB 的 ADPCM blob 以二进制附件方式编进固件。
#
# 为什么不生成一个巨大的 .c 数组：5 MB 的数组会让编译和链接明显变慢，生成出来的
# 源文件也无法审阅。二进制附件没有这两个问题，字节仍然落在 Flash 的 rodata 段，
# 读它就是一次内存映射访问。符号名由文件名决定（这里是 _binary_qpq_audio_bin_start），
# 声明集中在 main/qpq_audio_blob.c 一个文件里。
target_add_binary_data(${COMPONENT_LIB} "../assets/audio/qpq_audio.bin" BINARY)
