#include <ctype.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define metric_path "status.baseline.cfgchk.md5checksum_monitoring_for_windows_config_files"
#define phony_checksum_for_empty_file "00000000000000000000000000000000"
#define phony_checksum_for_non_previous "00000000000000000000000000000001"
#define REC_DIR "C:\\zabbix\\last_md5sum_monitoring_for_windows_config_files_md5"
#define ALL_REC_PATH "C:\\zabbix\\last_md5sum_monitoring_for_windows_config_files_md5\\all.dat"
#define DEFAULT_UUM "C:\\WINDOWS\\Security\\Database\\secedit.sdb"

// metric: status.baseline.cfgchk.md5sum_monitoring_for_windows_config_files
// values:
// 0: healthy, current md5 == previous md5
// 1: timeout when try to wait for secedit.sdb, file not found.
// 2: previous md5 value not found
// 3: current md5 != previous md5
// 4: failed to get md5checksum value of the target file.

// previous_checksum is passed with a char array with size set as 256
// 0-31 keep the checksum value, 32 is the end of string, and with the 33rd
// char, we keep a int value to identify if the recorded checksum need to be refreshed.
int run_chk(char *path, char *previous_checksum)
{
	 char buf[512], buf1[64];
	 struct stat st;
     FILE *f=NULL;
     int i=0, j, fd, r, flag = 0, res = -1;
	 
	 while((stat(path, &st) == -1)&&(i<3))
	 {
		 sleep(i);
		 i++;
	 }
	 if(stat(path, &st) == -1)
	 {
		 if(strcmp(previous_checksum, phony_checksum_for_non_previous) != 0)
		 {
			 previous_checksum[33]=1;
			 strcpy(path, "deleted");
			 strcpy(previous_checksum, phony_checksum_for_non_previous);
			 return 3;
		 }
		 else
		 {
			 previous_checksum[33]=0;
			 strcpy(path, "not found");
		     return 1;
		 }
	 }

	 if(st.st_size==0)
	 {
		 strcpy(buf1,phony_checksum_for_empty_file);
	 }
	 else
	 {
         sprintf(buf, "CertUtil -hashfile %s MD5", path);
		 //sprintf(buf, "powershell D:\\1.ps1 -hashfile %s MD5", path);
         f=popen(buf, "r");
	     fd=fileno(f);
         j = 0;
         while(((r=read(fd, buf, sizeof(buf)))>0)&&(res != 32))
         {
             buf[r]='\0';
             i = 0;
		     while(i<r)
		     {
			     // we find one new line
			     if((buf[i]=='\r')||(buf[i]=='\n'))
			     {
				     if((j==32)&&(j<sizeof(buf1)))
					 {
						 //printf("detected md5 value: %s\n", buf1);
					     buf1[j] = '\0';
						 res=32;
                         break;
				     }
					 else
					 {
						 j=0;
						 flag=0;
						 memset(buf1, '\0', sizeof(buf1));
					 }
			     }
			     else if(flag==0)
			     {
				     // strlenof(md5 value) is 32, and is composed of lowercase letters a-z and numbers 0-9, and may contains some spaces
				     // we drop the current line if it's too long( longer than 64 characters) or contains invalid characters
				     if((j>63)||(((buf[i]<'0')||(buf[i]>'9'))&&((buf[i]<'a')||(buf[i]>'z')) && !isspace(buf[i])))
				     {
						 j=0;
					     flag = 1;
					     memset(buf1, '\0', sizeof(buf1));
				     }
				     else
				     {
						 if(!isspace(buf[i]))
						 {
                             buf1[j] = buf[i];
				             j++;
                         }							 
				     }
			     }
                 i++;
		     }
         }
         close(fd);
         pclose(f);
     }

    if(strlen(buf1)==0)
	{
		//printf("Warning: failed to get md5 checksum value of file %s.\n", path);
		strcpy(buf1,phony_checksum_for_non_previous);
		previous_checksum[33]=1;
		res=4;
	}
	else if(strcmp(previous_checksum, phony_checksum_for_non_previous) == 0)
	{
		previous_checksum[33]=1;
		strcpy(path, "created");
		res = 3;
	}
	else
	{
		if(strcmp(previous_checksum,buf1))
		{
			previous_checksum[33]=1;
			strcpy(path, "changed");
			res = 3;
		}
		else
		{
			previous_checksum[33]=0;
			res = 0;
		}
	}
	//printf("wwwwwwww path:%s, buf1: %s\n",path, buf1);
	i=0;
	while((i<32)&&(i<strlen(buf1)))
	{
		previous_checksum[i]=buf1[i];
		i++;
	}
	previous_checksum[i]='\0';
	return res;
}

int main(int argc, char *argv[])
{
	const char *cfg_fname="\\secmd5chk.cfg";
    int i=0, res = 0, targets_cur=0, j;
	struct stat st;
	char buf[256], buf1[256], res_description[1024], cfg_path[256];
	char targets[32][256], targets_checksum[32][33];
 	char previous_targets[32][256], previous_targets_checksum[32][33];
	memset(cfg_path, '\0', 256);
	memset(targets, '\0', 32*256);
	memset(targets_checksum, '\0', 32*33);
    memset(previous_targets, '\0', 32*256);
	memset(previous_targets_checksum, '\0', 32*33);
	memset(res_description, '\0', 1024);
    getcwd(cfg_path,256);
	strcat(cfg_path,cfg_fname);

	if(stat("C:\\zabbix", &st) == -1)
		_mkdir("C:\\zabbix");
	if(stat(REC_DIR, &st) == -1)
		_mkdir(REC_DIR);

	if(stat(cfg_path, &st) == -1)
		 strcpy(targets[0],DEFAULT_UUM);
	else{
		 FILE *cfg_f = fopen(cfg_path, "r");
		 if( cfg_f == NULL )
		     strcpy(targets[0],DEFAULT_UUM);
		 else
		   {
		        memset(buf, '\0', sizeof(buf));
				targets_cur=0;
		        while(fgets(buf, 256, cfg_f))
		         {
					 if(buf[0]=='#')
						 continue;
					 i=0;
					 j=0;
					 while(isspace(buf[i]))
					     i++;
					 while((buf[i]!='\0')&&(j<255)&&(i<256)&&(buf[i]!='\n')&&(buf[i]!='\r')&&!isspace(buf[i]))
					 {
						 targets[targets_cur][j]=buf[i];
						 j++;
						 i++;
					 }
		             targets[targets_cur][j]='\0';

					 if(strlen(targets[targets_cur])>0)
					     targets_cur++;
					 else
						 targets[targets_cur][0]='\0';
		         }
		        fclose(cfg_f);
		   }
	}

    // Get the checksum values of the files we recorded when we checked them last time.
	if(stat(ALL_REC_PATH, &st) == 0)
	{
		FILE *rec_f = fopen(ALL_REC_PATH, "r");
		if( rec_f != NULL )
		{
		    memset(buf, '\0', sizeof(buf));
			targets_cur=0;
		    while(fgets(buf, 256, rec_f))
		    {
				i=0;
				while((buf[i]!=' ')&&(i<255))
				{
					previous_targets[targets_cur][i]=buf[i];
					i++;
				}
				previous_targets[targets_cur][i]='\0';
				j=0;
				i++;
				while((buf[i]!='\0')&&(j<32)&&(i<256)&&(buf[i]!='\n')&&(buf[i]!='\r'))
				{
					previous_targets_checksum[targets_cur][j]=buf[i];
					j++;
					i++;
				}
		        previous_targets_checksum[targets_cur][j]='\0';
				targets_cur++;
		    }
		    fclose(rec_f);
		}
	}

    targets_cur=0;
	j=0;
	while(targets[targets_cur][0] != '\0')
    {
		memset(buf, '\0', sizeof(buf));
		i=0;
		while(i<32)
		{
			if(strcmp(previous_targets[i], targets[targets_cur])==0)
			{
				strcpy(buf,previous_targets_checksum[i]);
				break;
			}
			i++;
		}
		if(strlen(buf)==0)
			strcpy(buf, phony_checksum_for_non_previous);
		strcpy(buf1, targets[targets_cur]);
		//printf("Invoking runchk with parameters buf1: %s, buf: %s\n", buf1,buf);
		if((run_chk(buf1,buf) == 3)||(strcmp(buf1, "not found")==0))
		{
			if((strlen(res_description)+strlen(buf1)+strlen(targets[targets_cur])+strlen(" was ; "))<1023)
			{
				strcat(res_description,targets[targets_cur]);
				strcat(res_description," was ");
				strcat(res_description,buf1);
				strcat(res_description,"; ");
			}
			else
			{
				res_description[1022]='.';
				res_description[1021]='.';
				res_description[1020]='.';
			}
			if(strcmp(buf1, "not found")!=0)
				res=1;
		}

		strcpy(targets_checksum[targets_cur],buf);
		j=j+(int)buf[33];
		targets_cur++;
	}
    // Update the checksum value records with the latest values if any of them has been changed.
	if(j>0)
	{
		FILE *f1 = fopen(ALL_REC_PATH, "w");
		if( f1 != NULL )
		{
			fseek(f1,0,SEEK_SET);
			targets_cur=0;
			while(targets[targets_cur][0] != '\0')
			{
		        fprintf(f1, "%s %s\n", targets[targets_cur], targets_checksum[targets_cur]);
				targets_cur++;
			}
		    fclose(f1);
		}			
	}
	if(strlen(res_description)==0)
		strcpy(res_description,"OK");
	printf("%d,%s\n",res, res_description);
	return res;
}

