#include "public.h"
#include <EEPROM.h>
struct FILE_INFO {
  char name[MAX_NAME_LEN];
  u16 start_address;
  u16 size;
  u8 state; //normal, read only, hide, opening delete?
  u8 xor_sum; //
};
typedef FILE_INFO *pFILE_INFO;
struct FS_HEAD {
  u8 fs_type; //file system type
  u8 fs_version; //for update migrate
  u8 sector_size; //default size is 1, for future use.
  u8 fs_file_table_address; //can added copy to table2 for safety
  char logo[8];
  u16 fs_file_start_address;
  u16 future;
};
static struct FS_HEAD fs_head = {
  0x01, 0x01, 0x01,   0x10, "AISTLFS", 0x0100, 0x0000
};

#define MAX_FILE_ADDRESS 1000

const u8 file_xor_sum_offset = 0xE5;
u16 file_tail = fs_head.fs_file_start_address;

u8 calc_xor_sum(FILE_INFO f) {
  u8 xor_sum = file_xor_sum_offset;
  u8 *p = (u8 *) &f;
  for (int i = 0; (p + i) < &f.xor_sum; i++) {
    xor_sum ^= *(p + i);
  }
  return xor_sum;
}

int is_valid_file(FILE_INFO f) {
  if (f.start_address < fs_head.fs_file_start_address)
    return 0;
  return calc_xor_sum(f) == f.xor_sum;
}

int find_file(char name[], FILE_INFO *fout, u16 *out_table_address) {
  int i, ffta = fs_head.fs_file_table_address;
  int len = (fs_head.fs_file_start_address - ffta) / sizeof(FILE_INFO);
  for (i = 0; i < len; i++)
  {
    FILE_INFO TMP;
    EEPROM.get(ffta, TMP);
    if (is_valid_file(TMP))
    {
      if (0 == strcmp(TMP.name, name))
      {
        memcpy(fout , &TMP, sizeof(FILE_INFO));
        return FILE_SUCCESS;
      }
    }
    else //for get empty file
    {
      *out_table_address = ffta;
      return FILE_NOT_EXIST;
    }
    ffta += sizeof(FILE_INFO);
  }
  return FILE_REACH_MAX;
}
static void _write_eeproms(u8 *buf, u16 address, u16 len)
{
  for (int i = 0; i < len; i++)
  {
    EEPROM[address + i] = buf[i];
  }
}
static void _set_eeproms(u8 value, u16 address, u16 len)
{
  for (int i = 0; i < len; i++)
  {
    EEPROM[address + i] = value;
  }
}
static void _read_eeproms(u8 *buf, u16 address, u16 len)
{
  for (int i = 0; i < len; i++)
  {
    buf[i] = EEPROM[address + i];
  }
}
int init_file_system() {
  EEPROM.put(0, fs_head);
  _set_eeproms(0, fs_head.fs_file_table_address, sizeof(FILE_INFO));
  return FILE_SUCCESS;
}

int read_file(char name[], u8 buf[]) {
  FILE_INFO TMP;
  u16 file_table_address;
  int ret = find_file(name, &TMP, &file_table_address);
  if (ret == FILE_SUCCESS)
  {
    _read_eeproms(buf, TMP.start_address, TMP.size);
  }
  return FILE_SUCCESS;
}

int write_file(char name[], u8 buf[], u8 len) {
  FILE_INFO TMP;
  u16 file_table_address;
  int ret = find_file(name, &TMP, &file_table_address);
  if (ret == FILE_NOT_EXIST)
  {
    TMP.start_address = file_tail;
    TMP.size = len;
    memcpy(TMP.name, name, strlen(name));
    TMP.xor_sum = calc_xor_sum(TMP);
    EEPROM.put(file_table_address, TMP);
    _write_eeproms(buf, TMP.start_address, TMP.size);
    file_tail += len;
  }

}

int delete_file(char name[]) {
  FILE_INFO TMP;
  u16 file_table_address;
  int ret = find_file(name, &TMP, &file_table_address);
  if (ret == FILE_SUCCESS)
  {
    TMP.xor_sum = ~TMP.xor_sum;
    EEPROM.put(file_table_address, TMP);
  }
  return FILE_SUCCESS;
}
