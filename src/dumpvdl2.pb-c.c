/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: dumpvdl2.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "dumpvdl2.pb-c.h"
void   dumpvdl2__vdl2_msg_metadata__timestamp__init
                     (Dumpvdl2__Vdl2MsgMetadata__Timestamp         *message)
{
  static const Dumpvdl2__Vdl2MsgMetadata__Timestamp init_value = DUMPVDL2__VDL2_MSG_METADATA__TIMESTAMP__INIT;
  *message = init_value;
}
void   dumpvdl2__vdl2_msg_metadata__init
                     (Dumpvdl2__Vdl2MsgMetadata         *message)
{
  static const Dumpvdl2__Vdl2MsgMetadata init_value = DUMPVDL2__VDL2_MSG_METADATA__INIT;
  *message = init_value;
}
size_t dumpvdl2__vdl2_msg_metadata__get_packed_size
                     (const Dumpvdl2__Vdl2MsgMetadata *message)
{
  assert(message->base.descriptor == &dumpvdl2__vdl2_msg_metadata__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t dumpvdl2__vdl2_msg_metadata__pack
                     (const Dumpvdl2__Vdl2MsgMetadata *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &dumpvdl2__vdl2_msg_metadata__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t dumpvdl2__vdl2_msg_metadata__pack_to_buffer
                     (const Dumpvdl2__Vdl2MsgMetadata *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &dumpvdl2__vdl2_msg_metadata__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Dumpvdl2__Vdl2MsgMetadata *
       dumpvdl2__vdl2_msg_metadata__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Dumpvdl2__Vdl2MsgMetadata *)
     protobuf_c_message_unpack (&dumpvdl2__vdl2_msg_metadata__descriptor,
                                allocator, len, data);
}
void   dumpvdl2__vdl2_msg_metadata__free_unpacked
                     (Dumpvdl2__Vdl2MsgMetadata *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &dumpvdl2__vdl2_msg_metadata__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   dumpvdl2__raw_avlc_frame__init
                     (Dumpvdl2__RawAvlcFrame         *message)
{
  static const Dumpvdl2__RawAvlcFrame init_value = DUMPVDL2__RAW_AVLC_FRAME__INIT;
  *message = init_value;
}
size_t dumpvdl2__raw_avlc_frame__get_packed_size
                     (const Dumpvdl2__RawAvlcFrame *message)
{
  assert(message->base.descriptor == &dumpvdl2__raw_avlc_frame__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t dumpvdl2__raw_avlc_frame__pack
                     (const Dumpvdl2__RawAvlcFrame *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &dumpvdl2__raw_avlc_frame__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t dumpvdl2__raw_avlc_frame__pack_to_buffer
                     (const Dumpvdl2__RawAvlcFrame *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &dumpvdl2__raw_avlc_frame__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Dumpvdl2__RawAvlcFrame *
       dumpvdl2__raw_avlc_frame__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Dumpvdl2__RawAvlcFrame *)
     protobuf_c_message_unpack (&dumpvdl2__raw_avlc_frame__descriptor,
                                allocator, len, data);
}
void   dumpvdl2__raw_avlc_frame__free_unpacked
                     (Dumpvdl2__RawAvlcFrame *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &dumpvdl2__raw_avlc_frame__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor dumpvdl2__vdl2_msg_metadata__timestamp__field_descriptors[2] =
{
  {
    "tv_sec",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_INT64,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata__Timestamp, tv_sec),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "tv_usec",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_INT64,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata__Timestamp, tv_usec),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned dumpvdl2__vdl2_msg_metadata__timestamp__field_indices_by_name[] = {
  0,   /* field[0] = tv_sec */
  1,   /* field[1] = tv_usec */
};
static const ProtobufCIntRange dumpvdl2__vdl2_msg_metadata__timestamp__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor dumpvdl2__vdl2_msg_metadata__timestamp__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "dumpvdl2.vdl2_msg_metadata.timestamp",
  "Timestamp",
  "Dumpvdl2__Vdl2MsgMetadata__Timestamp",
  "dumpvdl2",
  sizeof(Dumpvdl2__Vdl2MsgMetadata__Timestamp),
  2,
  dumpvdl2__vdl2_msg_metadata__timestamp__field_descriptors,
  dumpvdl2__vdl2_msg_metadata__timestamp__field_indices_by_name,
  1,  dumpvdl2__vdl2_msg_metadata__timestamp__number_ranges,
  (ProtobufCMessageInit) dumpvdl2__vdl2_msg_metadata__timestamp__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor dumpvdl2__vdl2_msg_metadata__field_descriptors[11] =
{
  {
    "station_id",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, station_id),
    NULL,
    &protobuf_c_empty_string,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "frequency",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, frequency),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "synd_weight",
    3,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, synd_weight),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "datalen_octets",
    4,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, datalen_octets),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "frame_pwr_dbfs",
    5,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_FLOAT,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, frame_pwr_dbfs),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "nf_pwr_dbfs",
    6,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_FLOAT,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, nf_pwr_dbfs),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "ppm_error",
    7,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_FLOAT,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, ppm_error),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "version",
    8,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_INT32,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, version),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "num_fec_corrections",
    9,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_INT32,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, num_fec_corrections),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "idx",
    10,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_INT32,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, idx),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "burst_timestamp",
    11,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__Vdl2MsgMetadata, burst_timestamp),
    &dumpvdl2__vdl2_msg_metadata__timestamp__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned dumpvdl2__vdl2_msg_metadata__field_indices_by_name[] = {
  10,   /* field[10] = burst_timestamp */
  3,   /* field[3] = datalen_octets */
  4,   /* field[4] = frame_pwr_dbfs */
  1,   /* field[1] = frequency */
  9,   /* field[9] = idx */
  5,   /* field[5] = nf_pwr_dbfs */
  8,   /* field[8] = num_fec_corrections */
  6,   /* field[6] = ppm_error */
  0,   /* field[0] = station_id */
  2,   /* field[2] = synd_weight */
  7,   /* field[7] = version */
};
static const ProtobufCIntRange dumpvdl2__vdl2_msg_metadata__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 11 }
};
const ProtobufCMessageDescriptor dumpvdl2__vdl2_msg_metadata__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "dumpvdl2.vdl2_msg_metadata",
  "Vdl2MsgMetadata",
  "Dumpvdl2__Vdl2MsgMetadata",
  "dumpvdl2",
  sizeof(Dumpvdl2__Vdl2MsgMetadata),
  11,
  dumpvdl2__vdl2_msg_metadata__field_descriptors,
  dumpvdl2__vdl2_msg_metadata__field_indices_by_name,
  1,  dumpvdl2__vdl2_msg_metadata__number_ranges,
  (ProtobufCMessageInit) dumpvdl2__vdl2_msg_metadata__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor dumpvdl2__raw_avlc_frame__field_descriptors[2] =
{
  {
    "metadata",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__RawAvlcFrame, metadata),
    &dumpvdl2__vdl2_msg_metadata__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "data",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_BYTES,
    0,   /* quantifier_offset */
    offsetof(Dumpvdl2__RawAvlcFrame, data),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned dumpvdl2__raw_avlc_frame__field_indices_by_name[] = {
  1,   /* field[1] = data */
  0,   /* field[0] = metadata */
};
static const ProtobufCIntRange dumpvdl2__raw_avlc_frame__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor dumpvdl2__raw_avlc_frame__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "dumpvdl2.raw_avlc_frame",
  "RawAvlcFrame",
  "Dumpvdl2__RawAvlcFrame",
  "dumpvdl2",
  sizeof(Dumpvdl2__RawAvlcFrame),
  2,
  dumpvdl2__raw_avlc_frame__field_descriptors,
  dumpvdl2__raw_avlc_frame__field_indices_by_name,
  1,  dumpvdl2__raw_avlc_frame__number_ranges,
  (ProtobufCMessageInit) dumpvdl2__raw_avlc_frame__init,
  NULL,NULL,NULL    /* reserved[123] */
};