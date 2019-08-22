#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>

void strtrunc(char *str, int len)
  {
    if ((len<1) || (strlen(str)<=len))
        return;
    *(str+len)='\0';
    if (len<8)
        return;
    *(str+len-1)='.';
    *(str+len-2)='.';
    *(str+len-3)='.';
  }

int get_line(int sock, char *buf, int size)
{
    int i = 0, j = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                  {
                    recv(sock, &c, 1, 0);
                    j++;
                  }
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i+j);
}

int is_ipv4_addr(char *ip_str)
{
  int res=0, cur=0, cur1=0, num=0;
  char buf[4];

  if ( ( *ip_str < '0' ) || ( *ip_str > '9' ) || (*(ip_str+strlen(ip_str)-1) < '0' ) || (*(ip_str+strlen(ip_str)-1) > '9' ) )
      return 0;

  memset(buf, '\0', 4);
  while (*(ip_str+cur) != '\0')
    {
      if ( (*(ip_str+cur) != '.') && ( (*(ip_str+cur) < '0') || (*(ip_str+cur) > '9') ) )
          return 0;
      else if (*(ip_str+cur) != '.')
        {
          if (cur1 > 2)
              return 0;
          buf[cur1] = *(ip_str+cur);
          cur1++;
        }
      else
        {
          if (cur1 == 0)
              return 0;
          cur1 = 0;
          num++;
          if (num > 3)
              return 0;
          if ( (atoi(buf) > 254) || (atoi(buf) < 1) )
              return 0;
        }
      cur++;
    }
  return 1;
}
