#ifndef UG_LIB_87DD87FE_8111_4485_B05F_805029047F81
#define UG_LIB_87DD87FE_8111_4485_B05F_805029047F81

#if ! defined __cplusplus
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#else
#include <cstddef>
#include <cstdint>
extern "C" {
#endif

#if (defined(_MSC_VER) || defined(__MINGW32__))
#ifdef LIBUG_EXPORT
#define LIBUG_DLL __declspec(dllexport)
#else
#define LIBUG_DLL __declspec(dllimport)
#endif
#else
#define LIBUG_DLL
#endif

#define DEFAULT_UG_PORT 5004

struct RenderPacket;
typedef void (*render_packet_received_callback_t)(void *udata, struct RenderPacket *pkt);

typedef enum {
        UG_RGBA = 1,     ///< RGBA 8-bit
        UG_I420 = 29,    ///< planar YUV 4:2:0 in one buffer
        UG_CUDA_I420 = 31, ///< I420 in CUDA buffer (Unified Memory pointer also possible)
        UG_CUDA_RGBA = 32, ///< RGBA 8-bit in CUDA buffer (Unified Memory pointer also possible)
} libug_pixfmt_t;

typedef enum {
        UG_UNCOMPRESSED = 0, ///< uncompressed input pixel format
        UG_JPEG,             ///< JPEG compression (from FFmpeg, CPU-backed)
} libug_compression_t;

struct ug_sender;

/**
 * Optional parameters must be set to zero
 */
struct ug_sender_parameters {
        const char *receiver;                    ///< receiver address
        int mtu;                                 ///< MTU (optional, default 1500)
        libug_compression_t compression;         ///< compression setting
        render_packet_received_callback_t rprc;  ///< callback for received position data (optional)
        void *rprc_udata;                        ///< user data passed to the rprc callback (optional)
        int port;                                ///< port (optional, default 5004)
        int verbose;                             ///< verbosity level (optional, default 0, 1 - verbose, 2 - debug)
        int enable_strips;                       ///< enable 8x1 strips (to improve compression), default 0 (disable)
        int connections;                         ///< number of connections (default 1), must match with receiver
        long long int traffic_shapper_bw;        ///< use specified bitrate for traffic shaper in bps (default 0 - unlimited)
        int cuda_device;                         ///< CUDA device to use - default 0
};

/**
 * @param init_params  @see ug_sender_parameters
 */
LIBUG_DLL struct ug_sender *ug_sender_init(const struct ug_sender_parameters *init_params);
/**
 * @brief Sends data buffer
 * @param seq   sequence number to be passed to VRG submit frame
 *
 * This function is blocking.
 */
LIBUG_DLL void ug_send_frame(struct ug_sender *state, const char *data, libug_pixfmt_t pixel_format, int width, int height, struct RenderPacket *pkt);
LIBUG_DLL void ug_sender_done(struct ug_sender *state);

struct ug_receiver;

/// Optional parameters must be set to zero
struct ug_receiver_parameters {
        const char *display;                     ///< display to use (optional, default vrg)
        const char *sender;                      ///< sender address for RTCP (optional)
        int port;                                ///< port for back channel (optional, default 5004; will use port + 1 for back channel)
        libug_pixfmt_t decompress_to;            ///< optional - pixel format to decompress to
        bool force_gpu_decoding;                 ///< force GPU decoding (decode with GPUJPEG)
        int verbose;                             ///< verbosity level (optional, default 0, 1 - verbose, 2 - debug)
        int enable_strips;                       ///< enable 8x1 strips (to improve compression), default 0 (disable)
        int connections;                         ///< number of connections (default 1), must match with sender
        int udp_packet_pool;                     ///< use UDP packet pool (default 0 - no) to increase recv performance, implies connections >= 1
        int cuda_device;                         ///< CUDA device to use - default 0
};
LIBUG_DLL struct ug_receiver *ug_receiver_start(struct ug_receiver_parameters *init_params);
LIBUG_DLL void ug_receiver_done(struct ug_receiver *state);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // defined UG_LIB_87DD87FE_8111_4485_B05F_805029047F81
