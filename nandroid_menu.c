#include <stdio.h>
#include <linux/input.h>
#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>

#include "common.h"
#include "recovery.h"
#include "recovery_ui.h"
#include "minui/minui.h"
#include "roots.h"
#include "strings.h"
#include "nandroid_menu.h"
#include "install_menu.h"

int sdext_present = 0;

void set_sdext() 
{
  sdext_present = 1;
}

void
nandroid (const char* operation, char *subname, char partitions, int reboot_after, int show_progress, int compress)
{
  ui_print ("Starting Nandroid %s.\n", operation);

  int boot = partitions & BOOT;
  int data = partitions & DATA;
  int cache = partitions & CACHE;
  int system = partitions & SYSTEM;
  int asecure = partitions & ASECURE;
  int sdext = partitions & SDEXT;
  int args;

  if (!strcmp(operation,"backup") == 0 ) args = 6;
  else args = 7;

  if (!boot) 
  {
    printf("\nBoot ignored.");
    args++;
  }
  if (!asecure)
  {
    printf("\nAndroid_secure ignored.");
    args++;
  }  
  if (!system)
  {
    printf("\nSystem ignored.");
    args++;
  }
  if (!data)
  {
    printf("\nData ignored.");
    args++;
  }  
  if (!cache)
  {
    printf("\nCache ignored.");
    args++;
  }
  if (!sdext)
  {
    printf("\nsd-ext ignored.");
    args++;
  }    
  if (show_progress)
  {
    printf("\nProgress shown.");
    args++;
  }  
  if (compress)
  {
    printf("\nCompression activated.");
    args++;
  } 
  if (reboot_after)
  {
    printf("\nReboot triggered.");
  }
  if (!strcmp (subname, ""))
  {
    printf("\nSubname given: %s", subname);
    args += 2;			// if a subname is specified, we need 2 more arguments
  }  

  char **argv = malloc (args * sizeof (char *));

  argv[0] = "/sbin/nandroid-mobile.sh";
  
  if (strcmp(operation,"backup") == 0) argv[1] = "--backup";
  if (strcmp(operation,"restore") == 0) argv[1] = "--restore";
  argv[2] = "--defaultinput";
  argv[args] = NULL;

  int i = 3;

  if (!boot)
	  {
	    argv[i++] = "--noboot";
	  }
  if (!asecure)
	  {
	    argv[i++] = "--nosecure";
	  }
  if (!system)
	  {
	    argv[i++] = "--nosystem";
	  }
  if (!data)
	  {
	    argv[i++] = "--nodata";
	  }
  if (!cache) 
  	  {
	    argv[i++] = "--nocache";
	  }
  if (!sdext) 
  	  {
	    argv[i++] = "--nosdext";
	  }
  if (show_progress)
	  {
	    argv[i++] = "--progress";
	  }
  if (compress)
	  {
	    argv[i++] = "--compress";
	  }	  
  if (strcmp (subname, ""))
	  {
	    argv[i++] = "--subname";
	    argv[i++] = subname;
	  }
  argv[i++] = NULL;

  char *envp[] = { NULL };

  int status = runve ("/sbin/nandroid-mobile.sh", argv, envp, 80);

  if (!WIFEXITED (status) || WEXITSTATUS (status) != 0)
    {
      if (!WIFEXITED (status) || WEXITSTATUS (status) == 34) 
      {
        if (ask_question("Out of space. Delete a backup?"))
        {  
          ui_reset_progress();
	  show_delete_menu();
        } else {
          ui_printf_int ("ERROR: Nandroid exited with status %d\n",
			   WEXITSTATUS (status));
        }
      }
    }  
    else
    {
      ui_print ("(done)\n");
       if (reboot_after) {
	 reboot_fn("android");
       }  
    }
  ui_reset_progress ();
}

void
nandroid_adv_r_choose_file (char *filename, char *nandroid_folder)
{
  static char *headers[] = { "Choose a backup",
    "",
    NULL
  };
  char path[PATH_MAX] = "";
  DIR *dir;
  struct dirent *de;
  int total = 0;
  int i;
  char **files;
  char **list;

  if (ensure_path_mounted (nandroid_folder) != 0)
	  {
	    LOGE ("Can't mount %s\n", nandroid_folder);
	    return;
	  }

  dir = opendir (nandroid_folder);
  if (dir == NULL)
	  {
	    LOGE ("Couldn't open directory %s\n", nandroid_folder);
	    return;
	  }

  while ((de = readdir (dir)) != NULL)
	  {
	    if (de->d_name[0] != '.')
		    {
		      total++;
		    }
	  }

  if (total == 0)
	  {
	    LOGE ("No nandroid backups found\n");
	    if (closedir (dir) < 0)
		    {
		      LOGE ("Failed to close directory %s\n", path);
		      goto out;	// eww, maybe this can be done better
		    }
	  }
  else
	  {
	    files = (char **) malloc ((total + 1) * sizeof (char *));
	    files[total] = NULL;

	    list = (char **) malloc ((total + 1) * sizeof (char *));
	    list[total] = NULL;

	    rewinddir (dir);

	    i = 0;
	    while ((de = readdir (dir)) != NULL)
		    {
		      if (de->d_name[0] != '.')
			      {
				files[i] =
				  (char *) malloc (strlen (nandroid_folder) +
						   strlen (de->d_name) + 1);
				strcpy (files[i], nandroid_folder);
				strcat (files[i], de->d_name);

				list[i] =
				  (char *) malloc (strlen (de->d_name) + 1);
				strcpy (list[i], de->d_name);

				i++;
			      }
		    }

	    if (closedir (dir) < 0)
		    {
		      LOGE ("Failure closing directory %s\n", path);
		      goto out;	// again, eww
		    }

	    sortlist(list, total); //sort it alphabetically
	    int chosen_item = -1;

	    while (chosen_item < 0)
		    {
		      chosen_item =
			get_menu_selection (headers, list, 0,
					    chosen_item <
					    0 ? 0 : chosen_item);
		      if (chosen_item >= 0 && chosen_item != ITEM_BACK)
			      {
				strcpy (filename, list[chosen_item]);
			      }
		    }
	  }
out:
  for (i = 0; i < total; i++)
	  {
	    free (files[i]);
	    free (list[i]);
	  }
  free (files);
  free (list);
}

void
reverse_array (char **inver_a)
{
  int j = sizeof (inver_a) / sizeof (char **);
  int i, temp;

  j--;
  for (i = 0; i < j; i++)
	  {
	    temp = inver_a[i];
	    inver_a[i] = inver_a[j];
	    inver_a[j] = temp;
	    j--;
	  }
  return inver_a;
}

void
get_nandroid_adv_r_menu_opts (char **list, char p, char *br, int reboot_after, int show_progress)
{

  char **tmp = malloc (8 * sizeof (char *));
  int i;

  for (i = 0; i < 7; i++)
	  {
	    tmp[i] =
	      malloc ((strlen ("(*)  Reboot afterwards") + strlen (br) +
		       1) * sizeof (char));
	  }
  int k = 0;
  sprintf (tmp[k], "(%c) %s BOOT", p & BOOT ? '*' : ' ', br); k++;
  sprintf (tmp[k], "(%c) %s DATA", p & DATA ? '*' : ' ', br); k++;
  sprintf (tmp[k], "(%c) %s ANDROID-SECURE", p & ASECURE ? '*' : ' ', br); k++;
  sprintf (tmp[k], "(%c) %s SYSTEM", p & SYSTEM ? '*' : ' ', br); k++;
  sprintf (tmp[k], "(%c) %s CACHE", p & CACHE ? '*' : ' ', br); k++;
if (sdext_present)
{
  sprintf (tmp[k], "(%c) %s SD-EXT", p & SDEXT ? '*' : ' ', br); k++;
}
  sprintf (tmp[k], "(%c) show progress", show_progress ? '*':' '); k++;
  sprintf (tmp[k], "(%c) reboot afterwards", reboot_after ? '*':' '); k++;

  tmp[k] = NULL;

  char **h = list;
  char **j = tmp;

  for (; *j; j++, h++)
    *h = *j;

}

void
get_nandroid_cmp_menu_opts (char **cmp_opts, int reboot_after)
{

  char **tmp = malloc (2 * sizeof (char *));
  int i;

  tmp[0] = malloc (strlen ("(*)  Reboot afterwards") * sizeof (char));

  sprintf (tmp[0], "(%c) reboot afterwards", reboot_after ? '*':' ');
  tmp[1] = NULL;

  char **h = cmp_opts;
  char **j = tmp;

  for (; *j; j++, h++)
    *h = *j;
}

void
get_nandroid_adv_b_menu_opts (char **list, char p, char *br, int reboot_after, int show_progress, int compress)
{
  int max_length;
  if (sdext_present) 
  {
    max_length = 10;
  }
  else
  { 
    max_length = 9;
  }
  
  char **tmp = malloc (max_length * sizeof (char *));
  int i;

  for (i = 0; i < max_length; i++)
	  {
	    tmp[i] =
	      malloc ((strlen ("(*)  Reboot afterwards") + strlen (br) +
		       1) * sizeof (char));
	  }
   
  int k = 0;
  sprintf (tmp[k], "(%c) %s BOOT", p & BOOT ? '*' : ' ', br); k++;
  sprintf (tmp[k], "(%c) %s DATA", p & DATA ? '*' : ' ', br); k++;
  sprintf (tmp[k], "(%c) %s ANDROID-SECURE", p & ASECURE ? '*' : ' ', br); k++;
  sprintf (tmp[k], "(%c) %s SYSTEM", p & SYSTEM ? '*' : ' ', br); k++;
  sprintf (tmp[k], "(%c) %s CACHE", p & CACHE ? '*' : ' ', br); k++;
if (sdext_present)
{
  sprintf (tmp[k], "(%c) %s SD-EXT", p & SDEXT ? '*' : ' ', br); k++;
}
  sprintf (tmp[k], "(%c) show progress", show_progress ? '*':' '); k++;
  sprintf (tmp[k], "(%c) gzip compress", compress ? '*':' '); k++;
  sprintf (tmp[k], "(%c) reboot afterwards", reboot_after ? '*':' '); k++;

  tmp[k] = NULL;

  char **h = list;
  char **j = tmp;

  for (; *j; j++, h++)
    *h = *j;

}

void
show_nandroid_adv_r_menu ()
{
  char *headers[] = { "Choose partitions to restore",
    "prefix:",
    "",
    "",
    NULL
  };

  char *items_nosd[] = { "Perform restore",
    "Choose backup",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };
  
  char *items_sd[] = { "Perform restore",
    "Choose backup",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
	NULL,
    NULL,
    NULL
  };

  char filename[PATH_MAX];

  filename[0] = NULL;
  char partitions = (char) DEFAULT;
  int chosen_item = -1;
  int show_progress = 1;
  int reboot_after = 0;
  int compress = 0;

  while (chosen_item != ITEM_BACK)
	  {
	    if (sdext_present)
		{
		  get_nandroid_adv_r_menu_opts (items_sd + 2, partitions, "restore", reboot_after, show_progress);	// put the menu options in items[] starting at index 2
	      chosen_item =
	        get_menu_selection (headers, items_sd, 0,
				  chosen_item < 0 ? 0 : chosen_item);

	      switch (chosen_item)
		    {
		    case 0:
		      nandroid("restore", filename, partitions, reboot_after, show_progress, compress);
		      break;
		    case 1:
		      nandroid_adv_r_choose_file (filename,
						  "/sdcard/nandroid");
		      headers[2] = filename;
		      break;
		    case 2:
		      partitions ^= BOOT;
		      break;
		    case 3:
		      partitions ^= DATA;
		      break;
		    case 4:
		      partitions ^= ASECURE;
		      break;
		    case 5:
		      partitions ^= SYSTEM;
		      break;
		    case 6:
		      partitions ^= CACHE;
		      break;  
			case 7: 
			  partitions ^= SDEXT;
			  break;
		    case 8:
		      show_progress ^= 1;
		      break;
		    case 9:
		      reboot_after ^= 1;
		      break;
		    }
		}
		else
		{
		  get_nandroid_adv_r_menu_opts (items_sd + 2, partitions, "restore", reboot_after, show_progress);	// put the menu options in items[] starting at index 2
	      chosen_item =
	        get_menu_selection (headers, items_sd, 0,
				  chosen_item < 0 ? 0 : chosen_item);

	      switch (chosen_item)
		    {
		    case 0:
		      nandroid("restore", filename, partitions, reboot_after, show_progress, compress);
		      break;
		    case 1:
		      nandroid_adv_r_choose_file (filename,
						  "/sdcard/nandroid");
		      headers[2] = filename;
		      break;
		    case 2:
		      partitions ^= BOOT;
		      break;
		    case 3:
		      partitions ^= DATA;
		      break;
		    case 4:
		      partitions ^= ASECURE;
		      break;
		    case 5:
		      partitions ^= SYSTEM;
		      break;
		    case 6:
		      partitions ^= CACHE;
		      break;
		    case 7:
		      show_progress ^= 1;
		      break;
		    case 8:
		      reboot_after ^= 1;
		      break;
			}
		}
	}		
}

void
show_delete_menu() 
{
  char *headers[] = { "Choose a backup to delete",
    "Backup:",
    "",
    "",
    NULL
  };

  char *items[] = { "Choose backup",
    "Delete!",
    NULL
  };

#define D_ITEM_C  0
#define D_ITEM_D  1
  
  char filename[PATH_MAX];
  filename[0] = NULL;

  char operation[PATH_MAX];
  char del_cmd[PATH_MAX];
  int chosen_item = -1;

  while (chosen_item != ITEM_BACK) 
  {
    chosen_item = get_menu_selection(headers, items, 0, chosen_item < 0 ? 0 : chosen_item);
  
    switch (chosen_item) 
    {
      case D_ITEM_C:
        nandroid_adv_r_choose_file (filename, "/sdcard/nandroid");
        headers[2] = filename;
	sprintf(operation, "Delete %s", filename);
	sprintf(del_cmd,"rm -rf /sdcard/nandroid/%s", filename);
        break;
      case D_ITEM_D:  
        if (confirm_selection("Are you sure?", operation, 0))
        { 
	if (strcmp(filename,"") != 0) 
	  {  
            ui_print("Deleting %s...\n", filename);
	    ui_show_indeterminate_progress();
	    __system(del_cmd);
	    ui_print("Done!\n", filename);
	  } else {
	    ui_print("You must select a backup first!\n");
	  }  
        } else {
	  return;
	}
	ui_reset_progress();
        return;
    }
  }  
}


void
show_compress_menu() 
{
  char *headers[] = { "Choose a backup to compress",
    "Backup:",
    "",
    "",
    NULL
  };

  char *cmp_opts[] = { "Choose backup",
    "Compress!",
	NULL,
    NULL
  };

#define C_ITEM_C  0
#define C_ITEM_D  1
#define C_ITEM_R  2
   
  int reboot_after = 0;
  
  char filename[PATH_MAX];
  filename[0] = NULL;

  char operation[PATH_MAX];
  char cmp_cmd[PATH_MAX];
  int chosen_item = -1;

  while (chosen_item != ITEM_BACK) 
  {
    get_nandroid_cmp_menu_opts (cmp_opts + 2, reboot_after);	// put the menu options in cmp_opts[] starting at index 2
    chosen_item = get_menu_selection(headers, cmp_opts, 0, chosen_item < 0 ? 0 : chosen_item);
  
    switch (chosen_item) 
    {
      case C_ITEM_C:
        nandroid_adv_r_choose_file (filename, "/sdcard/nandroid");
        headers[2] = filename;
	
		sprintf(operation, "Compress %s", filename);
		char pathname[256];
		sprintf(pathname, "/sdcard/nandroid/%s", filename);
		char **argv = malloc (2 * sizeof (char *));

		argv[0] = "/sbin/compress_nandroid.sh";
		argv[1] = pathname;
		argv[2] = NULL;
	
		char *envp[] = { NULL };
		break;
      case C_ITEM_D:  
		if (strcmp(filename,"") != 0) 
		{  
			int status = runve("/sbin/compress_nandroid.sh", argv, envp, 200);
			if (status == 100) 
			{
			  return;
			}
			if (status == 0)
			{
				if (reboot_after) reboot_fn("android");
			}
			ui_reset_progress();			
		} else {
			ui_print("You must select a backup first!\n");
		}  
        return;
	  case C_ITEM_R:
		reboot_after ^= 1;
		break;
    }
  }  
}

void
show_nandroid_adv_b_menu ()
{
  char *headers[] = { "Choose partitions to backup",
    "",
    NULL
  };
  
  //this next part is way ugly but I'm not yet good enough to do dynamic memory allocation for sd-ext
  char *items_nosd[] = { "Perform backup",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };


  char *items_sd[] = { "Perform backup",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };
  
  char filename[PATH_MAX];

  filename[0] = NULL;
  int chosen_item = -1;
  int show_progress = 1;
  int reboot_after = 0;
  int compress = 0;

  char partitions = (char) DEFAULT;

  while (chosen_item != ITEM_BACK)
	  {
	    if (sdext_present)
	    {
	      get_nandroid_adv_b_menu_opts (items_sd + 1, partitions, "backup", reboot_after, show_progress, compress);	// put the menu options in items[] starting at index 1
		  	    chosen_item =
	      get_menu_selection (headers, items_sd, 0,
				  chosen_item < 0 ? 0 : chosen_item);
	      switch (chosen_item)
		    {
		    case 0:
		      nandroid("backup", filename, partitions, reboot_after, show_progress, compress);
		      break;
		    case 1:
		      partitions ^= BOOT;
		      break;
		    case 2:
		      partitions ^= DATA;
		      break;
		    case 3:
		      partitions ^= ASECURE;
		      break;
		    case 4:
		      partitions ^= SYSTEM;
		      break;
		    case 5:
		      partitions ^= CACHE;
		      break;
			case 6:
			  partitions ^= SDEXT;
			  break;
		    case 7:
		      show_progress ^= 1;
		      break;
		    case 8:
		      compress ^= 1;
		      break;
		    case 9:
		      reboot_after ^= 1;
		      break;
		    }
	    }
	    else
	    {
	      get_nandroid_adv_b_menu_opts (items_nosd + 1, partitions, "backup", reboot_after, show_progress, compress);	// put the menu options in items[] starting at index 1
		  	    chosen_item =
	      get_menu_selection (headers, items_nosd, 0,
				  chosen_item < 0 ? 0 : chosen_item);
				  	      switch (chosen_item)
		    {
		    case 0:
		      nandroid("backup", filename, partitions, reboot_after, show_progress, compress);
		      break;
		    case 1:
		      partitions ^= BOOT;
		      break;
		    case 2:
		      partitions ^= DATA;
		      break;
		    case 3:
		      partitions ^= ASECURE;
		      break;
		    case 4:
		      partitions ^= SYSTEM;
		      break;
		    case 5:
		      partitions ^= CACHE;
		      break;
		    case 6:
		      show_progress ^= 1;
		      break;
		    case 7:
		      compress ^= 1;
		      break;
		    case 8:
		      reboot_after ^= 1;
		      break;
		    }
	    }
	}
}


void
show_nandroid_menu ()
{
  if (volume_present("/sd-ext"))
  {
    set_sdext();
  }

  static char *headers[] = { "Nandroid Menu",
    "",
    NULL
  };
  static char *items[] = { "Nandroid Backup",
    "Nandroid Restore",
    "Compress existing backup",
    "Delete backup",
    NULL
  };

#define ITEM_ADV_BACKUP  0
#define ITEM_ADV_RESTORE 1
#define ITEM_COMPRESS    2
#define ITEM_DELETE	 3

  int chosen_item = -1;

  while (chosen_item != ITEM_BACK)
	  {
	    chosen_item =
	      get_menu_selection (headers, items, 0,
				  chosen_item < 0 ? 0 : chosen_item);

	    switch (chosen_item)
		    {
		    case ITEM_ADV_BACKUP:
		      show_nandroid_adv_b_menu ();
		      break;
		    case ITEM_ADV_RESTORE:
		      show_nandroid_adv_r_menu ();
		      break;
		    case ITEM_COMPRESS:
		      show_compress_menu();
		      break;
		    case ITEM_DELETE:
		      show_delete_menu();
		      break;
		    }
	  }
}
