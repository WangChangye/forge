#include <ctype.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define metric_path "status.baseline.cfgchk.md5checksum_monitoring_for_windows_config_files"

// metric: status.baseline.cfgchk.md5sum_monitoring_for_windows_config_files
// values:
// 0: healthy, current md5 == previous md5
// 1: timeout when try to wait for secedit.sdb, file not found.
// 2: previous md5 value not found
// 3: current md5 != previous md5
// 4: failed to get md5checksum value of the target file.

int run_chk(char *path, char *rec_path)
{
	 char buf[1024], buf1[128];
	 struct stat st;
     FILE *f=NULL, *f1=NULL;
     int i=0, j, fd, r, start=0, end = 0, flag = 0, res = -1;
	 
	 while((stat(path, &st) == -1)&&(i<4))
	 {
		 sleep(2*i);
		 i++;
	 }
	 if(stat(path, &st) == -1)
	 {
		 
		 if(stat(rec_path, &st) == 0)
		 {
			 remove(rec_path);
			 printf("Warning: file %s monitored by secmd5chk was deleted recently.\n",path);
			 return 3;
		 }
		 else
		 {
			 printf("Warning: file %s monitored by secmd5chk is missing.\n",path);
		     return 1;
		 }
	 }

	 if(st.st_size==0)
	 {
		 strcpy(buf1,"00000000000000000000000000000000");
	 }
	 else
	 {
         sprintf(buf, "CertUtil -hashfile %s MD5", path);
         f=popen(buf, "r");
	     fd=fileno(f);
         j = 0;
         while(((r=read(fd, buf, sizeof(buf)))>0)&&(res<0))
         {
             buf[r]='\0';
             i = 0;
		     while((i<r)&&(res<0))
		     {
			     // we find one new line
			     if((buf[i]=='\r')||(buf[i]=='\n'))
			     {
				     flag = 0;
				     if((j>0)&&(j<sizeof(buf1)))
					     buf1[j] = '\0';
				     j = 0;
				     if(strlen(buf1)>0)
				     {
					     //printf("detected md5 value: %s\n", buf1);
                         res=32;
				     }
			     }
			     else if(!flag)
			     {
				     // strlenof(md5 value) is 32, and is composed of lowercase letters a-z and numbers 0-9
				     // we drop the current line if it's too long or contains invalid characters
				     if((j>127)||(((buf[i]<'0')||(buf[i]>'9'))&&((buf[i]<'a')||(buf[i]>'z')) && !isspace(buf[i])))
				     {
					     flag = 1;
					     buf1[0] = '\0';
				     }
				     else
				     {
                         buf1[j] = buf[i];
				         j++;					
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
		printf("Warning: failed to get md5 checksum vaue of file %s.\n", path);
		strcpy(buf1,"00000000000000000000000000000000");
		res=4;
	}
	else if(stat(rec_path, &st) == -1)
	{
		res = 2;
	}
	else
	{
		f1 = fopen(rec_path, "r");
		if( f1 == NULL )
		{
			//printf(metric_path": 2\n");
			res = 2;
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 128, f1);
		fclose(f1);
		while((buf[strlen(buf)-1] == '\n') || (buf[strlen(buf)-1] == '\r'))
			buf[strlen(buf)-1] = '\0';
        //printf("buf: %s|, buf1: %s|\n", buf, buf1);
		if(strcmp(buf,buf1))
		{
			printf("Warning: md5 checksum of file %s has been changed recently.\n", path);
			res = 3;
		}
		else
		{
			//printf(metric_path": 0\n");
			res = 0;
		}
	}
	if((res==2)||(res==3))
	{
		f1 = fopen(rec_path, "w");
		if( f1 == NULL )
			remove(rec_path);
		else
		{
		    memset(buf, '\0', sizeof(buf));
		    fseek(f1,0,SEEK_SET);
		    fprintf(f1, "%s", buf1);
		    fclose(f1);
		}			
	}
	return res;
}

int main(int argc, char *argv[])
{
	const char *cfg_fname="\\secmd5chk.cfg";
    int i=0, res = 0, targets_cur=0;
	struct stat st;
	char buf[256], cfg_path[256], targets[32][256], target_ids[32][8];
	memset(cfg_path, '\0', 256);
	memset(targets, '\0', 32*256);
	memset(target_ids, '\0', 32*8);
    getcwd(cfg_path,256);
	strcat(cfg_path,cfg_fname);

	if(stat("C:\\zabbix", &st) == -1)
		_mkdir("C:\\zabbix");
	if(stat("C:\\zabbix\\last_md5sum_monitoring_for_windows_config_files_md5", &st) == -1)
		_mkdir("C:\\zabbix\\last_md5sum_monitoring_for_windows_config_files_md5");

	if(stat(cfg_path, &st) == -1)
	 {
		 strcpy(targets[0],"C:\\WINDOWS\\Security\\Database\\secedit.sdb");
		 strcpy(target_ids[0],"rec");
	 }
	else{
		 FILE *cfg_f = fopen(cfg_path, "r+");
		 if( cfg_f == NULL )
		   {
		     strcpy(targets[0],"C:\\WINDOWS\\Security\\Database\\secedit.sdb");
		     strcpy(target_ids[0],"rec");
		   }
		 else
		   {
		        memset(buf, '\0', sizeof(buf));
				targets_cur=0;
		        while(fgets(buf, 256, cfg_f))
		         {
					 if(buf[0]=='#')
						 continue;
					 i=0;
					 while(buf[i]!=',')
					 {
						 target_ids[targets_cur][i]=buf[i];
						 i++;
					 }
					 target_ids[targets_cur][i]='\0';
		             strcpy(targets[targets_cur],buf+i+1);
					 i=strlen(targets[targets_cur])-1;
					 while((i>0)&&((isspace(targets[targets_cur][i]))||(targets[targets_cur][i]=='\n')))
					 {
					    targets[targets_cur][i]='\0';
                        i--;							 
					 }
					 if((strlen(targets[targets_cur])>0)&&(strlen(target_ids[targets_cur])>0))
					     targets_cur++;
					 else
					 {
						 targets[targets_cur][0]='\0';
						 target_ids[targets_cur][0]='\0';
					 }
		         }
		        fclose(cfg_f);
		   }
	}
    targets_cur=0;
	while(targets[targets_cur][0] != '\0')
    {
		memset(buf, '\0', sizeof(buf));
		sprintf(buf,"C:\\zabbix\\last_md5sum_monitoring_for_windows_config_files_md5\\%s.dat",target_ids[targets_cur]);
		//printf("wwwwwwwww target:%s,  rec_path: %s\n", targets[targets_cur], buf);
		if(run_chk(targets[targets_cur],buf) == 3)
			res++;
		targets_cur++;
	}
	printf(metric_path": %d\n",res);
	return res;
}

