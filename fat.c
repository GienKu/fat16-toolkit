#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

bool compare(unsigned char *entry_filename, char *filename)
{
  int entry_len = 0;
  int filename_len = strlen(filename);

  // Calculate the actual length of entry_filename (excluding spaces)
  for (int i = 0; i < 8; i++)
  {
    if (entry_filename[i] == ' ')
    {
      break;
    }
    entry_len++;
  }

  // Check if filename is shorter than entry_filename
  if (filename_len != entry_len)
  {
    return false;
  }

  // Compare each character up to the length of entry_filename
  for (int i = 0; i < entry_len; i++)
  {
    if (entry_filename[i] != filename[i])
    {
      return false;
    }
  }

  // // If entry_filename has more characters, they should be spaces
  // for (int i = entry_len; i < 8; i++) {
  //     if (entry_filename[i] != ' ') {
  //         return false;
  //     }
  // }

  return true;
}

void print_dir(FILE *in, Fat16BootSector bs, PartitionTable *pt, unsigned int cluster, int tabs, bool recursively)
{
  Fat16Entry entry;
  long long pos;
  unsigned int startRoot = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats) * bs.sector_size;
  unsigned int fatTableStart = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1) * 512;
  unsigned int dataSectorStart = startRoot + bs.root_dir_entries * sizeof(Fat16Entry);
  unsigned short int fatEntry = cluster;

  // Read all entries of directory
  while (fatEntry != 0xFFFF)
  {
    // read cluster
    fseek(in, dataSectorStart + (cluster - 2) * bs.sector_size * (bs.sectors_per_cluster), SEEK_SET);
    // fgetpos(in, &t_pos);

    for (int c = 0; c < bs.sectors_per_cluster; c++)
    {
      for (int p = 0; p < bs.sector_size; p += sizeof(Fat16Entry))
      {
        fread(&entry, sizeof(Fat16Entry), 1, in);
        if (entry.filename[0] != 0x00 && entry.filename[0] != 0xE5)
        {
          for (int t = 0; t < tabs; t++)
          {
            printf(" - ");
          }
          // for(int v = 0; v < 8; v++)
          //   printf("%c",entry.filename[v]);
          printf("%.8s.%.3s attr: 0x%02X cluster %8d len %8d B\n", entry.filename, entry.ext, entry.attributes, entry.starting_cluster, entry.file_size);
          if (entry.attributes == 0x10 && entry.filename[0] != 46 && recursively) // for . and .. folders
          {
            // don't know if it will work because there is no more nested dirs in my sd card image
            pos = fgetpos(in, &pos); // preserve current position  when func returns
            print_dir(in, bs, pt, entry.starting_cluster, tabs + 1, true);
            fseek(in, pos, SEEK_SET);
          }
        }
      }
    }
    fseek(in, fatTableStart + sizeof(short int) * (fatEntry), SEEK_SET);
    // fgetpos(in, &t_pos);

    fread(&fatEntry, sizeof(short int), 1, in);
    cluster = fatEntry;
  }
}

void print_tree(FILE *in, Fat16BootSector bs, PartitionTable *pt)
{
  Fat16Entry entry;
  int tabs = 0;
  long long pos;
  unsigned int startRoot = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats) * bs.sector_size;
  fseek(in, startRoot, SEEK_SET);
  printf("\nFilesystem directory listing\n-----------------------\n");
  for (int i = 0; i < bs.root_dir_entries; i++)
  {
    fread(&entry, sizeof(entry), 1, in);
    if (entry.filename[0] != 0x00 && entry.filename[0] != 0xE5)
    {
      printf("%.8s.%.3s attr: 0x%02X cluster %8d len %8d B\n", entry.filename, entry.ext, entry.attributes, entry.starting_cluster, entry.file_size);
      if (entry.attributes == 0x10)
      {
        pos = fgetpos(in, &pos); // preserve current position  when func returns
        print_dir(in, bs, pt, entry.starting_cluster, tabs + 1, true);
        // comeback to the right place after reading dirs
        fseek(in, startRoot + ((i + 1) * (sizeof(Fat16Entry))), SEEK_SET);
      }
    }
  }
}

void delete_file_from_fat(FILE *in, Fat16BootSector bs, PartitionTable *pt, unsigned int start_cluster)
{
  //todo rewrite this to actually work...
  unsigned int fatTableStart = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1) * 512;
  unsigned int fatTableCluster = start_cluster;
  while (fatTableCluster != 0xFFFF)
  {
    fseek(in, fatTableStart + sizeof(short int) * fatTableCluster, SEEK_SET);
    fread(&fatTableCluster, sizeof(short int), 1, in);
    fwrite("\0\0", 1, 2, in);
  }
  fwrite("\0\0", 1, 2, in);
}

unsigned int rec_look_for_file(FILE *in, Fat16BootSector bs, PartitionTable *pt, char *token, unsigned int cluster, bool all_files)
{
  // printf("\n%s\n", token);
  if (token == NULL)
    return cluster;

  Fat16Entry entry;
  long long pos;
  unsigned int startRoot = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats) * bs.sector_size;
  unsigned int fatTableStart = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1) * 512;
  unsigned int dataSectorStart = startRoot + bs.root_dir_entries * sizeof(Fat16Entry);
  unsigned short int fatEntry = cluster;
  // Read all entries of directory
  while (fatEntry != 0xFFFF)
  {
    // read cluster
    fseek(in, dataSectorStart + (cluster - 2) * bs.sector_size * (bs.sectors_per_cluster), SEEK_SET);
    // fgetpos(in, &t_pos);

    for (int c = 0; c < bs.sectors_per_cluster; c++)
    {
      for (int p = 0; p < bs.sector_size; p += sizeof(Fat16Entry))
      {
        fread(&entry, sizeof(Fat16Entry), 1, in);
        if (entry.attributes == 0x10 || all_files)
        {
          if (entry.filename[0] != 0x00 && entry.filename[0] != 0xE5 && compare(entry.filename, token))
          {
            token = strtok(NULL, "/");
            return rec_look_for_file(in, bs, pt, token, entry.starting_cluster, all_files);
          }
        }
      }
    }
    fseek(in, fatTableStart + sizeof(short int) * (fatEntry), SEEK_SET);
    // fgetpos(in, &t_pos);

    fread(&fatEntry, sizeof(short int), 1, in);
    cluster = fatEntry;
  }
  return -1;
}

// function returns a cluster where the directory data starts
// all_files means to look for dirs and files when true, when false then only dirs
unsigned int look_for_file(FILE *in, Fat16BootSector bs, PartitionTable *pt, char *path, bool all_files, bool shouldBeDeleted)
{
  Fat16Entry entry;
  char *token;
  token = strtok(path, "/");
  printf("\n%s\n", token);
  if (token != NULL)
  {

    unsigned int startRoot = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats) * bs.sector_size;
    fseek(in, startRoot, SEEK_SET);
    for (int i = 0; i < bs.root_dir_entries; i++)
    {
      fread(&entry, sizeof(entry), 1, in);
      if (entry.attributes == 0x10 || all_files)
      {
        if (entry.filename[0] != 0x00 && entry.filename[0] != 0xE5 && compare(entry.filename, token))
        {
          // after recursion is going back our pointer in file is still right after entry we were looking for
          // to reduce repetitive code that will traverse again the directories we can use this fact here if we want to delete file
          // and set the filename to 0x00
          // to not delete too early we check our pointer `token` which will hold NULL after whole path has been traversed and it reached final entry
          token = strtok(NULL, "/");
          unsigned int c = rec_look_for_file(in, bs, pt, token, entry.starting_cluster, all_files);
          if (shouldBeDeleted)
          {
            long long pos;
            fgetpos(in,&pos);
            // fseek(in,1, SEEK_CUR);
            Fat16Entry temp;
            fread(&temp, sizeof(Fat16Entry), 1, in);
            // fwrite("\0", 1, 1, in);
            // delete_file_from_fat(in, bs, pt, c);
            printf("DELETED");
          }
          return c;
        }
      }
    }
  }
  printf("Invlid path");

  return -1;
}

void read_file(FILE *in, Fat16BootSector bs, PartitionTable *pt, unsigned int fileCluster)
{
  long long pos;
  unsigned int startRoot = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats) * bs.sector_size;
  unsigned int fatTableStart = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1) * 512;
  unsigned int dataSectorStart = startRoot + bs.root_dir_entries * sizeof(Fat16Entry);

  unsigned short int fatFileEntry = fileCluster;
  char buff[512];

  // Read all entries of root directory
  while (fatFileEntry != 0xFFFF)
  {
    fseek(in, dataSectorStart + (fileCluster - 2) * bs.sector_size * (bs.sectors_per_cluster), SEEK_SET);
    fgetpos(in, &pos);

    for (int i = 0; i < bs.sector_size * bs.sectors_per_cluster; i += bs.sector_size)
    {
      fread(buff, sizeof(char), bs.sector_size, in);
      for (int p = 0; p < bs.sector_size; p++)
      {
        printf("%c", buff[p]);
      }
    }
    fseek(in, fatTableStart + sizeof(short int) * fatFileEntry, SEEK_SET);
    // fgetpos(in, &t_pos);

    fread(&fatFileEntry, sizeof(short int), 1, in);
    fileCluster = fatFileEntry;
  }
  fseek(in, fatTableStart + sizeof(short int) * fatFileEntry, SEEK_SET);
  // fgetpos(in, &t_pos);

  fread(&fatFileEntry, sizeof(short int), 1, in);
  fileCluster = fatFileEntry;
}

void read_root_file(FILE *in, Fat16BootSector bs, PartitionTable *pt, char *filename)
{
  Fat16Entry entry;
  long long t_pos;
  unsigned int startRoot = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats) * bs.sector_size;
  unsigned int fatTableStart = 512 * pt[0].start_sector + sizeof(Fat16BootSector) + (bs.reserved_sectors - 1) * 512;
  unsigned int dataSectorStart = startRoot + bs.root_dir_entries * sizeof(Fat16Entry);

  fseek(in, startRoot, SEEK_SET);

  // Read all entries of root directory
  for (int i = 0; i < bs.root_dir_entries; i++)
  {
    fread(&entry, sizeof(entry), 1, in);

    if (compare(entry.filename, filename) && entry.filename[0] != 0x00 && entry.filename[0] != 0xE5)
    {
      printf("\nFound filename... reading...\n");
      unsigned short int fatEntry = entry.starting_cluster;
      unsigned int cluster = entry.starting_cluster;
      unsigned char buff[512];

      while (fatEntry != 0xFFFF)
      {
        // read cluster

        fseek(in, dataSectorStart + (cluster - 2) * bs.sector_size * (bs.sectors_per_cluster), SEEK_SET);
        // fgetpos(in, &t_pos);

        for (int c = 0; c < bs.sectors_per_cluster; c++)
        {
          fread(buff, sizeof(char), bs.sector_size, in);
          for (int p = 0; p < bs.sector_size; p++)
          {
            printf("%c", buff[p]);
          }
        }
        fseek(in, fatTableStart + sizeof(short int) * (fatEntry), SEEK_SET);
        // fgetpos(in, &t_pos);

        fread(&fatEntry, sizeof(short int), 1, in);
        cluster = fatEntry;
      }
    }
  }
}

int main()
{
  FILE *in = fopen("sd.img", "rb+");
  int i;
  PartitionTable pt[4];
  Fat16BootSector bs;
  Fat16Entry entry;

  fseek(in, 0x1BE, SEEK_SET);               // go to partition table start, partitions start at offset 0x1BE
  fread(pt, sizeof(PartitionTable), 4, in); // read all entries (4)

  printf("Partition table\n-----------------------\n");
  for (i = 0; i < 4; i++)
  { // for all partition entries print basic info
    printf("Partition %d, type %02X, ", i, pt[i].partition_type);
    printf("start sector %8d, length %8d sectors\n", pt[i].start_sector, pt[i].length_sectors);
  }

  printf("\nSeeking to first partition by %d sectors\n", pt[0].start_sector);
  fseek(in, 512 * pt[0].start_sector, SEEK_SET); // Boot sector starts here (seek in bytes)
  fread(&bs, sizeof(Fat16BootSector), 1, in);    // Read boot sector content
  printf("Volume_label %.11s, %d sectors size\n", bs.volume_label, bs.sector_size);
 
  // printf("\nReading ABSTRACT.TXT contents\n-----------------------\n");
  // read_file(in, bs, pt, "ABSTRAKT");
  print_tree(in, bs, pt);
  // char dir_path[] = "ADR1/";
  // unsigned int dir_cluster = look_for_file(in, bs, pt, dir_path, false, false);
  // printf("\n dir cluster = %d\n", dir_cluster);

  // if (dir_cluster != -1)
  // {
  //   print_dir(in, bs, pt, dir_cluster, 0, false);
  // }

  char file_path[] = "ADR2/HISTORIE";
  // unsigned int file_cluster = look_for_file(in, bs, pt, file_path, true, false);
  // printf("\n file cluster = %d\n", file_cluster);
  // if (file_cluster != -1)
  // {
  //   read_file(in, bs, pt, file_cluster);
  //   // read_root_file(in, bs, pt, "ABSTRAKT");
  // }
  // delete_file(in, bs, pt, file_path);
  look_for_file(in, bs, pt, file_path, true, true);
  print_tree(in, bs, pt);
  fclose(in);
  return 0;
}
