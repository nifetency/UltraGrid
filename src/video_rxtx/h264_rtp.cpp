/**
 * @file   video_rxtx/h264_rtp.cpp
 * @author Martin Pulec     <pulec@cesnet.cz>
 * @author David Cassany    <david.cassany@i2cat.net>
 * @author Ignacio Contreras <ignacio.contreras@i2cat.net>
 * @author Gerard Castillo  <gerard.castillo@i2cat.net>
 */
/*
 * Copyright (c) 2013-2014 Fundació i2CAT, Internet I Innovació Digital a Catalunya
 * Copyright (c) 2013-2024 CESNET, z. s. p. o.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of CESNET nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "compat/strings.h" // strdupa
#include "debug.h"
#include "host.h"
#include "lib_common.h"
#include "rtp/rtp.h"
#include "rtp/rtpenc_h264.h"
#include "transmit.h"
#include "tv.h"
#include "utils/color_out.h"
#include "video.h"
#include "video_rxtx.hpp"
#include "video_rxtx/h264_rtp.hpp"

#define MOD_NAME "[vrxtx/h264_rtp] "

using std::shared_ptr;

h264_rtp_video_rxtx::h264_rtp_video_rxtx(std::map<std::string, param_u> const &params,
                int rtsp_port) :
        rtp_video_rxtx(params)
{
        rtsp_params.rtsp_port = (unsigned) rtsp_port;
        rtsp_params.parent = static_cast<struct module *>(params.at("parent").ptr);
        rtsp_params.avType = static_cast<rtsp_types_t>(params.at("avType").l);
        rtsp_params.audio_codec = static_cast<audio_codec_t>(params.at("audio_codec").l);
        rtsp_params.audio_sample_rate = params.at("audio_sample_rate").i;
        rtsp_params.audio_channels = params.at("audio_channels").i;
        rtsp_params.audio_bps = params.at("audio_bps").i;
        rtsp_params.rtp_port = params.at("rx_port").i;  //server rtp port
        rtsp_params.rtp_port_audio = params.at("a_rx_port").i;
        rtsp_params.video_codec = H264;
}

void
h264_rtp_video_rxtx::send_frame(shared_ptr<video_frame> tx_frame) noexcept
{
        if (m_rtsp_server == nullptr) {
                if (tx_frame->color_spec == H264) {
                        tx_send_std = tx_send_h264;
                } else if (tx_frame->color_spec == JPEG ||
                           tx_frame->color_spec == MJPG) {
                        tx_send_std = tx_send_jpeg;
                } else {
                        MSG(ERROR,
                            "codecs other than H.264 and JPEG currently not "
                            "supported, got %s\n",
                            get_codec_name(tx_frame->color_spec));
                        return;
                }
                rtsp_params.video_codec = tx_frame->color_spec;
                m_rtsp_server = c_start_server(rtsp_params);
        }

        tx_send_std(m_tx, tx_frame.get(), m_network_device);

        if ((m_rxtx_mode & MODE_RECEIVER) == 0) { // send RTCP (receiver thread would otherwise do this
                time_ns_t curr_time = get_time_in_ns();
                uint32_t ts = (curr_time - m_start_time) / 100'000 * 9; // at 90000 Hz
                rtp_update(m_network_device, curr_time);
                rtp_send_ctrl(m_network_device, ts, nullptr, curr_time);

                // receive RTCP
                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 0;
                rtp_recv_r(m_network_device, &timeout, ts);
        }
}

h264_rtp_video_rxtx::~h264_rtp_video_rxtx()
{
        free(m_rtsp_server);
}

void h264_rtp_video_rxtx::join()
{
        c_stop_server(m_rtsp_server);
        video_rxtx::join();
}

static void rtps_server_usage(){
        printf("\n[RTSP SERVER] usage:\n");
        color_printf("\t" TBOLD("-x rtsp[:port=number]") "\n");
        printf("\t\tdefault rtsp server port number: 8554\n\n");

        /// see audio_tx_send_standard()
        color_printf("Supported audio codecs: " TBOLD("MP3") ", " TBOLD(
            "Opus") ", " TBOLD("PCMA") " (A-law), " TBOLD("PCMU") " (u-law)\n");
        color_printf(
            "Supported video codecs: " TBOLD("H.264") ", " TBOLD("JPEG") "\n");
        color_printf("\n");
}

static int get_rtsp_server_port(const char *config) {
        if (strncmp(config, "port:", 5) != 0 &&
            strncmp(config, "port=", 5) != 0) {
                log_msg(LOG_LEVEL_ERROR, "\n[RTSP SERVER] ERROR - please, check usage.\n");
                rtps_server_usage();
                return -1;
        }
        if (strlen(config) == 5) {
                log_msg(LOG_LEVEL_ERROR, "\n[RTSP SERVER] ERROR - please, enter a port number.\n");
                rtps_server_usage();
                return -1;
        }
        if (config[4] == ':') {
                MSG(WARNING, "deprecated usage - use port=number, not port:number!\n");
        }
        int port = atoi(config + 5);
        if (port < 0 || port > 65535 || !isdigit(config[5])) {
                log_msg(LOG_LEVEL_ERROR, "\n[RTSP SERVER] ERROR - please, enter a valid port number.\n");
                rtps_server_usage();
                return -1;
        }
        return port;
}

static video_rxtx *create_video_rxtx_h264_std(std::map<std::string, param_u> const &params)
{
        int rtsp_port;
        const char *rtsp_port_str = static_cast<const char *>(params.at("opts").ptr);
        if (strlen(rtsp_port_str) == 0) {
                rtsp_port = 0;
        } else {
                if (strcmp(rtsp_port_str, "help") == 0) {
                        rtps_server_usage();
                        return nullptr;
                }
                rtsp_port = get_rtsp_server_port(rtsp_port_str);
                if (rtsp_port == -1) {
                        return nullptr;
                }
        }
        return new h264_rtp_video_rxtx(params, rtsp_port);
}

static const struct video_rxtx_info h264_video_rxtx_info = {
        "H264 standard",
        create_video_rxtx_h264_std
};

REGISTER_MODULE(rtsp, &h264_video_rxtx_info, LIBRARY_CLASS_VIDEO_RXTX, VIDEO_RXTX_ABI_VERSION);

