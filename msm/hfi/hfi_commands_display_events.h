/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __H_HFI_COMMANDS_DISPLAY_EVENTS_H
#define __H_HFI_COMMANDS_DISPLAY_EVENTS_H

/*
 * This is documentation file. Not used for header inclusion.
 */

/*
 * All Event level commands begin here.
 * "1st MSB byte = 0x04"
 */
#define HFI_COMMAND_DISPLAY_EVENT_BEGIN                                         0x04000000

/*
 * hfi_header sample for any event command:
 * hfi_header.cmd_buff_info.size          : (sizeof(hfi_header) +
 *                                            sizeof(hfi_packet)
 *           .cmd_buff_info.type          : HFI_DISPLAY
 *           .device_id                   : 0
 *           .object_id                   : n/a
 *           .timestamp_hi                : ts_hi
 *           .timestamp_lo                : ts_lo
 *           .header_id                   : unique id
 *           .num_packets                 : 'n' packets depending upon the
 *                                          packet cmd
 *
 * hfi_packet sample for any event packet:
 * hfi_packet.payload_info.size           : sizeof(hfi_packet)
 *                                          (including payload size)
 *           .payload_info.type           : one of enum
 *                                          'hfi_packet_payload_type'
 *           .cmd                         : HFI_COMMAND_DISPLAY_EVENT_XXX
 *           .flags (Host to DCP)         : HFI_TX_FLAGS_XXX
 *                                          (enum hfi_packet_host_flags)
 *                  (DCP to Host)         : HFI_RX_FLAGS_XXX
 *                                          (enum hfi_fw_host_flags)
 *           .id                          : 0
 *           .packet_id                   : unique id
 */

/*
 * HFI_COMMAND_DISPLAY_EVENT_VSYNC - This is a DCP event command sent to Host to notify vsync
 *                                   timestamp and number of HW vsync count.
 * Data layout:
 * struct hfi_display_vsync_data - vsync data
 * @timestamp_lo    :  lower value of 64bit vsync timestamp
 * @timestamp_hi    :  higher value of 64bit vsync timestamp
 * @vsync_index     :  vsync index for the timestamp
 *  struct hfi_display_vsync_data {
 *   u32 timestamp_lo;
 *   u32 timestamp_hi;
 *   u32 vsync_index;
 *  }
 * hfi_packet.payload_info.type           : HFI_PAYLOAD_U32_ARRAY
 *           .cmd                         : HFI_COMMAND_DISPLAY_EVENT_VSYNC
 *           .flags                       : HFI_RX_FLAGS_NONE
 *           .id                          : BITS 0:15 carry the display id for which the event
 *                                          is applicable
 *           .payload[0-3]                : struct hfi_display_vsync_data
 */
#define HFI_COMMAND_DISPLAY_EVENT_VSYNC                                         0x04000001

/*
 * HFI_COMMAND_DISPLAY_EVENT_FRAME_SCAN_START- This is a DCP event command sent to Host to notify
 *                                     timestamp when the frame starts getting scanned by display
 *                                     and the buffer flip index for the corresponding timestamp.
 * Data layout:
 * struct hfi_display_frame_event_data- frame event data
 * @timestamp_lo      :  lower value of 64bit Buffer flip timestamp
 * @timestamp_hi      :  higher value of 64bit Buffer flip timestamp
 * @bufferflip_index  :  carry the "sequence_id" received from
 *                       HFI_PROPERTY_DISPLAY_SCAN_SEQUENCE_ID,
 *                       otherwise it will be zero.
 * struct hfi_display_frame_event_data{
 *   u32 timestamp_lo;
 *   u32 timestamp_hi;
 *   u32 bufferflip_index;
 *  }
 * hfi_packet.payload_info.type           : HFI_PAYLOAD_U32_ARRAY
 *           .cmd                         : HFI_COMMAND_DISPLAY_EVENT_FRAME_SCAN_START
 *           .flags                       : HFI_RX_FLAGS_NONE
 *           .id                          : BITS 0:15 carry the display id for which the event
 *                                          is applicable
 *           .payload[0-3]                : struct hfi_display_frame_event_data
 */
#define HFI_COMMAND_DISPLAY_EVENT_FRAME_SCAN_START                              0x04000002

/*
 * HFI_COMMAND_DISPLAY_EVENT_FRAME_SCAN_COMPLETE - This is a DCP event command sent to Host to
 *                                                 notify when the frame processing is done.
 * hfi_packet.payload_info.type           : HFI_PAYLOAD_U32_ARRAY
 *           .cmd                         : HFI_COMMAND_DISPLAY_EVENT_FRAME_SCAN_COMPLETE
 *           .flags                       : HFI_RX_FLAGS_NONE
 *           .id                          : BITS 0:15 carry the display id for which the event
 *                                          is applicable
 *           .payload[0-3]                : struct hfi_display_frame_event_data
 */
#define HFI_COMMAND_DISPLAY_EVENT_FRAME_SCAN_COMPLETE                           0x04000003

/*
 * HFI_COMMAND_DISPLAY_EVENT_IDLE - This is a DCP event command sent to Host to notify
 * when display is idle.
 *
 * Below table describes the hfi_packet layout (Only data that would change per command is listed
 * below, other fields can be found in @ref disp_events_header_data_page)
 *
 * Data layout:
 * struct hfi_display_idle_event_data - idle event data
 * @timestamp_lo      :  lower value of 64bit idle timestamp
 * @timestamp_hi      :  higher value of 64bit idle timestamp
 * @idle_index        :  number of idle timeouts since last enable/disable of display
 * struct hfi_display_idle_event_data{
 *   u32 timestamp_lo;
 *   u32 timestamp_hi;
 *   u32 idle_index;
 *  }
 *
 * Hfi packet layout                      | Value
 *----------------------------------------|---------------------------------
 * hfi_packet.payload_info (type)         | HFI_PAYLOAD_U32_ARRAY
 * hfi_packet.cmd                         | HFI_COMMAND_DISPLAY_EVENT_IDLE
 * hfi_packet.flags                       | HFI_RX_FLAGS_NONE
 * hfi_packet.id                          | BITS 0:15 carry the display id for which the event is
						applicable
 * hfi_packet.payload[0-3]                | struct hfi_display_idle_event_data
 */
#define HFI_COMMAND_DISPLAY_EVENT_IDLE                                          0x04000004

/*
 * HFI_COMMAND_DISPLAY_EVENT_POWER - This is a DCP event command sent to Host to notify per
 *                                   display power on/off.
 * Data layout:
 * struct hfi_display_power_event_data - power event data
 * @timestamp_lo      :  lower value of 64bit idle timestamp
 * @timestamp_hi      :  higher value of 64bit idle timestamp
 * @power_state       :  power_state corresponding to which power mode we are in.
 * struct hfi_display_power_event_data{
 *   u32 timestamp_lo;
 *   u32 timestamp_hi;
 *   enum hfi_display_power_mode power_state;
 *  }
 *
 * Below table describes the hfi_packet layout (Only data that would change per command is listed
 * below, other fields can be found in @ref disp_events_header_data_page)
 *
 * Hfi packet layout                      | Value
 *----------------------------------------|---------------------------------
 * hfi_packet.payload_info (type)         | HFI_PAYLOAD_U32_ARRAY
 * hfi_packet.cmd                         | HFI_COMMAND_DISPLAY_EVENT_POWER
 * hfi_packet.flags                       | HFI_RX_FLAGS_NONE
 * hfi_packet.id                          | BITS 0:15 carry the display id for which the event is
						applicable
 * hfi_packet.payload[0-3]                | struct hfi_display_power_event_data
 */
#define HFI_COMMAND_DISPLAY_EVENT_POWER                                         0x04000005

/*
 * HFI_COMMAND_DISPLAY_EVENT_HW_RECOVERY is sent from DCP to host to indicate a hw recovery event
 * for a given display.
 *
 * DCP to Host:
 * hfi_header.num_packets                 : 1
 *
 * hfi_packet.payload_info.type        : HFI_PAYLOAD_NONE
 *           .cmd                      : HFI_COMMAND_DISPLAY_EVENT_HW_RECOVERY
 *           .flags                    : HFI_TX_FLAGS_RESPONSE_REQUIRED
 *           .id                       : BITS 0:15 carry the display id for which the event
 *                                          is applicable
 */
#define HFI_COMMAND_DISPLAY_EVENT_HW_RECOVERY                                   0x04000006

#define HFI_COMMAND_DISPLAY_EVENT_END                                           0x04FFFFFF

#endif // __H_HFI_COMMANDS_DISPLAY_EVENTS_H
