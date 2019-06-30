#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/wait.h>
#include <stdlib.h>

#define panic() assert(0)

const char *IMG_FILE = "/home/stardustdl/temp/fat/fs.img";
const char *TARGET_DIR = "/home/stardustdl/temp/fat/imgs";

enum DirentryProp
{
  DP_RW = 0,
  DP_RO = 1,
  DP_Hidden = 2,
  DP_System = 4,
  DP_Volume = 8,
  DP_Sub = 16,
  DP_Arch = 32,
  DP_Long = 0x0f,
};

typedef struct
{
  uint8_t jump_code[3];
  char oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sector_per_cluster;
  uint16_t reserved_sector_count;
  uint8_t fat_count;
  uint16_t root_dir_entry_count;
  uint16_t total_sector_16;
  uint8_t media;
  uint16_t fat_size_16;
  uint16_t sector_per_track;
  uint16_t heads_count;
  uint32_t hidden_sectors;
  uint32_t total_sector_32;
  uint32_t fat_size_32;
  uint16_t external_flags;
  uint16_t fs_version;
  uint32_t root_dir_cluster;
  uint16_t fs_info;
  uint16_t boot_record_backup;
  uint8_t reserved[12];
  uint8_t driver_count;
  uint8_t reserved2;
  uint8_t boot_signature;
  uint32_t volume_id;
  char volume_label[11];
  char fs_type[8];
} __attribute__((packed)) PBR;

uint32_t get_fat_area_offset(PBR *pbr, uint32_t fat_id)
{
  uint32_t offset = pbr->bytes_per_sector * (pbr->reserved_sector_count + fat_id * pbr->fat_size_32);
  return offset;
}

uint32_t get_data_area_offset(PBR *pbr)
{
  uint32_t offset = pbr->bytes_per_sector * (pbr->reserved_sector_count + pbr->fat_count * pbr->fat_size_32);
  return offset;
}

uint32_t get_cluster_offset(PBR *pbr, uint32_t cluster_id)
{
  uint32_t offset = get_data_area_offset(pbr) + pbr->bytes_per_sector * pbr->sector_per_cluster * (cluster_id - 2);
  return offset;
}

typedef struct
{
  char name[8];
  char ext[3];
  uint8_t prop;
  uint8_t reserved;
  uint8_t create_time_ms;
  uint16_t create_time;
  uint16_t create_date;
  uint16_t access_date;
  uint16_t cluster_high;
  uint16_t modify_time;
  uint16_t modify_date;
  uint16_t cluster_low;
  uint32_t size;
} __attribute__((packed)) ShortDirentry;

typedef struct
{
  uint8_t prop;
  char name1[10];
  uint8_t flag;
  uint8_t reserved;
  uint8_t checksum;
  char name2[12];
  uint16_t cluster;
  char name3[4];
} __attribute__((packed)) LongDirentry;

int is_last_long_entry(LongDirentry *entry)
{
  return (entry->prop) >> 6 & 1;
}

uint8_t get_long_entry_index(LongDirentry *entry)
{
  return ((entry->prop) << 3) >> 3;
}

uint32_t get_cluster_index(ShortDirentry *entry)
{
  return entry->cluster_high << 16 | entry->cluster_low;
}

void *load_image(const char *image_path, uint32_t *size)
{
  int fd = open(image_path, O_RDONLY);
  if (fd < 0)
  {
    fprintf(stderr, "open: %s\n", strerror(errno));
    panic();
  }
  struct stat stat;
  fstat(fd, &stat);
  *size = stat.st_size;
  void *res = mmap(NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (res == MAP_FAILED)
  {
    fprintf(stderr, "mmap: %s\n", strerror(errno));
    panic();
  }
  return res;
}

void putnchar(char *str, int n)
{
  while (n > 0 && *str != '\0')
  {
    putchar(*str);
    str++;
    n--;
  }
}

PBR *detect_fat(const void *img)
{
  PBR *pbr = (PBR *)img;

  /*
  puts("Detected FAT32 filesystem:");
  printf("  OEM: ");
  putnchar(pbr->oem_name, 8);
  puts("");
  printf("  File system type: ");
  putnchar(pbr->fs_type, 8);
  puts("");
  printf("  Volume label: ");
  putnchar(pbr->volume_label, 11);
  puts("");
  printf("  Volume id: %u\n", pbr->volume_id);
  printf("  Bytes per sector: %u\n", pbr->bytes_per_sector);
  printf("  Sector per cluster: %u\n", pbr->sector_per_cluster);
  printf("  Reserved sector count: %u\n", pbr->reserved_sector_count);
  printf("  Hidden sectors: %u\n", pbr->hidden_sectors);
  printf("  Number of FATs: %u\n", pbr->fat_count);
  printf("  Total sector: %u\n", pbr->total_sector_32);
  printf("  FAT size: %u\n", pbr->fat_size_32);
  printf("  Root directory cluster: %#x\n", pbr->root_dir_cluster);
  */

  return pbr;
}

int count1(uint32_t x)
{
  if (x == 0)
    return 0;
  return (x & 1) + count1(x >> 1);
}

void detect_direntry_raw(PBR *pbr, void *start, uint32_t size)
{
  for (uint32_t pos = 0; pos < size; pos += 32)
  {
    void *cur = start + pos;
    ShortDirentry *sh = (ShortDirentry *)cur;
    if (sh->prop == DP_Long)
    {
      LongDirentry *lo = (LongDirentry *)cur;
      if (lo->reserved != 0 || lo->cluster != 0)
        continue;
      printf("Long entry detected at %u.\n", pos);
    }
    else
    {
      if (sh->reserved != 0 || count1(sh->prop) != 1)
        continue;
      printf("Short entry detected at %u.\n", pos);
      printf("  Name: ");
      putnchar(sh->name, 8);
      puts("");
      printf("  Extension: ");
      putnchar(sh->ext, 3);
      puts("");
      printf("  Cluster: %u -> (%#x)\n", get_cluster_index(sh), get_cluster_offset(pbr, get_cluster_index(sh)));
      printf("  Size: %u\n", sh->size);
      if (sh->prop == DP_Sub)
      {
        puts("  Is a subdir.");
      }
    }
  }
}

typedef struct
{
  char name[512];
  uint32_t size;
  uint32_t cluster;
  uint32_t shid;
} RecoverFile;

int detect_next_direntry(void *start, uint32_t offset, uint32_t size, RecoverFile *file)
{
  static char temp[512];
  int temp_pos = 0;
  for (uint32_t pos = offset; pos < size; pos += 32)
  {
    void *cur = start + pos;
    ShortDirentry *sh = (ShortDirentry *)cur;
    if (sh->prop == DP_Long)
    {
      LongDirentry *lo = (LongDirentry *)cur;
      if (lo->reserved != 0 || lo->cluster != 0)
        continue;
      if (temp_pos == 0 && is_last_long_entry(lo) == 0)
      {
        continue;
      }
      for (int i = 2; i >= 0; i -= 2)
        temp[temp_pos++] = lo->name3[i];
      for (int i = 10; i >= 0; i -= 2)
        temp[temp_pos++] = lo->name2[i];
      for (int i = 8; i >= 0; i -= 2)
        temp[temp_pos++] = lo->name1[i];
    }
    else
    {
      if (sh->reserved != 0 || count1(sh->prop) != 1 || sh->ext[0] != 'B' || sh->ext[1] != 'M' || sh->ext[2] != 'P')
      {
        // temp_pos = 0;
        continue;
      }
      file->size = sh->size;
      file->cluster = get_cluster_index(sh);
      if (temp_pos > 0)
      {
        for (int jpos = 0; jpos < temp_pos; jpos++)
        {
          file->name[jpos] = temp[temp_pos - 1 - jpos];
        }
        file->name[temp_pos] = '\0';
      }
      else
      {
        for (int jpos = 0; jpos < 8; jpos++)
        {
          file->name[jpos] = sh->name[jpos];
        }
        file->name[8] = '.';
        for (int jpos = 9; jpos < 12; jpos++)
        {
          file->name[jpos] = sh->ext[jpos - 9];
        }
        file->name[12] = '\0';
      }
      file->shid = pos;
      return pos + 32;
    }
  }
  return -1;
}

void getSha1Sum(void *src, uint32_t size, char *buf, uint32_t buf_size)
{
  static char *arg[3] = {"sha1sum", "-", NULL};

  int pf[2], pf2[2];
  if (pipe(pf) != 0)
  {
    fprintf(stderr, "pipe: %s\n", strerror(errno));
    panic();
  }
  if (pipe(pf2) != 0)
  {
    fprintf(stderr, "pipe: %s\n", strerror(errno));
    panic();
  }

  int pid = fork();

  if (pid == 0)
  {
    // dup2(open("/dev/null", O_RDWR), STDERR_FILENO);
    dup2(pf[1], STDOUT_FILENO);
    close(pf[0]);
    close(pf[1]);
    dup2(pf2[0], STDIN_FILENO);
    close(pf2[1]);
    close(pf2[0]);

    execvp("sha1sum", arg);
  }
  else
  {
    close(pf[1]);
    close(pf2[0]);
    write(pf2[1], src, size);
    close(pf2[1]);
    int status;
    waitpid(pid, &status, 0);
    read(pf[0], buf, 40);
    buf[40] = '\0';
    close(pf[0]);
  }
}

typedef struct
{
  char checksum[128];
  char name[512];
  int rank;
} Answer;

Answer answers[10240];
int answer_cnt = 0;

int cmp(const void *a, const void *b)
{
  return ((Answer *)a)->rank - ((Answer *)b)->rank;
}

void recover(void *img, uint32_t img_size, PBR *pbr, int tofile)
{
  static char output_file[1024];
  static char sha1_buf[512];

  answer_cnt = 0;

  uint32_t data_offset = get_data_area_offset(pbr);
  uint32_t pos = 0, size = img_size - data_offset;
  RecoverFile file;
  while (pos < size)
  {
    pos = detect_next_direntry(img + data_offset, pos, size, &file);
    if (pos == -1)
      break;
    if (file.size == 0 || file.cluster == 0)
      continue;

    /*
    printf("BMP detected: %s\n", file.name);
    printf("  Size: %u\n", file.size);
    printf("  Cluster: %u -> (%#x)\n", file.cluster, get_cluster_offset(pbr, file.cluster));
    printf("  Short entry position: %u\n", file.shid);
    */

    void *src_ptr = img + get_cluster_offset(pbr, file.cluster);
    assert(*(char *)src_ptr == 'B');
    assert(*(char *)(src_ptr + 1) == 'M');

    if (tofile)
    {
      sprintf(output_file, "%s/%s", TARGET_DIR, file.name);
      int dst_fd = open(output_file, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
      if (dst_fd < 0)
      {
        fprintf(stderr, "create: %s %s\n", output_file, strerror(errno));
        panic();
      }
      truncate(output_file, file.size);
      void *dst_ptr = mmap(NULL, file.size, PROT_WRITE | PROT_READ, MAP_SHARED, dst_fd, 0);
      if (dst_ptr == MAP_FAILED)
      {
        fprintf(stderr, "mmap: %s\n", strerror(errno));
        panic();
      }

      memcpy(dst_ptr, src_ptr, file.size);

      munmap(dst_ptr, file.size);
      close(dst_fd);
    }
    getSha1Sum(src_ptr, file.size, sha1_buf, sizeof(sha1_buf));

    // printf("%s  %s\n", sha1_buf, file.name);
    strcpy(answers[answer_cnt].checksum, sha1_buf);
    strcpy(answers[answer_cnt].name, file.name);
    if (strstr(file.name, "~") != NULL)
    {
      answers[answer_cnt].rank = 1;
    }
    else
    {
      answers[answer_cnt].rank = 0;
    }
    answer_cnt++;
  }
  qsort(answers, answer_cnt, sizeof(Answer), cmp);
  for (int i = 0; i < answer_cnt; i++)
  {
    printf("%s  %s\n", answers[i].checksum, answers[i].name);
  }
}

void *img;
uint32_t img_size;
PBR *pbr;

int main(int argc, char *argv[])
{
  img = load_image(argv[1], &img_size);
  pbr = detect_fat(img);
  // detect_direntry_raw(pbr, img + data_offset, img_size - data_offset);
  recover(img, img_size, pbr, 0);
  return 0;
}
