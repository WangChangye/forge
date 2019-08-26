#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "fnt_http_fun.h"

#define ISspace(x) isspace((int)(x))
#define SERVER_STRING "Server: fnt_httpd/0.1.0\r\n"
#define REQ_PAR_ACTION 0
#define REQ_PAR_ID 1

void discard_headers(int);
void accept_request(int);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, char * const*, const char *, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void handle_post_req(int);
void resp_msg(int, char *, char *, char *);

FILE *fp;

struct request{
    int mask;
    char method[8];
    char url[128];
    char data[1024];
    int content_len;
};

struct req_args{
    int len;
    char keys[16][32];
    char values[16][32];
};

struct request parse_req(int client)
  {
    char buf[128],buf1[8];
    struct request rh;
    rh.mask=0;
    int n=0, i, j;
    n = get_line(client, buf, sizeof(buf));
    i=0;
    while ( (i<strlen(buf)) && !ISspace(buf[i]) && (i<7))
      {
        rh.method[i] = buf[i];
        i++;
      }
    rh.method[i] = '\0';
    if(i>0)
        rh.mask=rh.mask|1;
    while ((i<strlen(buf)) && ISspace(buf[i]))
        i++;
    j=0;
    while ( (i<strlen(buf)) && !ISspace(buf[i]) && (j<127))
      {
        rh.url[j] = buf[i];
        i++;
        j++;
      }
    rh.url[j] = '\0';
    if(j>0)
        rh.mask=rh.mask|2;
    while ((n > 0) && strcmp("\n", buf))
      {
        printf("< %s",buf);
        j=strlen(buf);
        if(j+strlen(rh.data)<1024)
            memcpy(rh.data+strlen(rh.data),buf,j);
        i=0;
        while((i<j)&&(buf[i]!=':'))
            i++;
        if(i>(j-2))
          {
            n = get_line(client, buf, sizeof(buf));
            continue;
          }
        buf[i]='\0';
        i++;
        while((i<j)&&(ISspace(buf[i])))
            i++;
        n=i;
        while((i<j)&&(!ISspace(buf[i])))
            i++;
        if((n<i)&&(i<j))
          {
            buf[i]='\0';
            if(strcmp(buf,"Content-Length")==0)
               {
                  rh.content_len=atoi(&(buf[i]));
                  if(rh.content_len>0)
                      rh.mask=rh.mask|4;
               }
          }
        n = get_line(client, buf, sizeof(buf));
      }
    return rh;
  }

struct req_args parse_url(char *url)
{
    int i = 0, n = strlen(url)-2, cur0;
    struct req_args res;
    res.len=0;
    memset(res.keys,'\0',16*32*sizeof(char));
    memset(res.values,'\0',16*32*sizeof(char));
    if(strlen(url)>(2*16*32))
    {
        res.len=-1;
        return res;   //Exception: arg is too long or without end character '\0'.
    }
    while ((ISspace(url[i])) && (i < strlen(url)))
        i++;
    while ( (n>=i) && (ISspace(url[n])) )
        n--;
    if (n < i) // no none-space character found in url
        return res;
    else
        url[n+1]='\0';

    cur0 = i;
    while (i<=n)
    {
        if ( ((i-cur0+1)>32)||(res.len>16) ) // Exception: length of k/v excceeds array size 16*31
        {
            res.len=-1;
            return res;
        }
        if ( (url[i]=='?') && (res.len == 0) )
        {
            url[i] = '\0';
            strcpy(res.keys[0],"path\0");
            strcpy(res.values[0],url+cur0);
            cur0=i+1;
            res.len++;
        }
        else if ( (url[i]=='=') && (res.keys[res.len][0]=='\0') && (res.values[res.len][0]=='\0')  )
        {
            url[i] = '\0';
            strcpy(res.keys[res.len],url+cur0);
            cur0=i+1;            
        }
        else if ( (url[i]=='&') && (res.keys[res.len][0]!='\0') && (res.values[res.len][0]=='\0') )
        {
            url[i] = '\0';
            strcpy(res.values[res.len],url+cur0);
            cur0=i+1;
            res.len++;
        }
        else if (i==n)
        {
            if ( (res.keys[res.len][0]!='\0') && (res.values[res.len][0]=='\0') )
                strcpy(res.values[res.len],url+cur0);
            if ( (res.values[res.len][0]=='\0') || (res.keys[res.len][0]=='\0') )
                res.len--;
        }
        i++;
    }
    return res;
}
/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client)
{
    char buf[1024], buf_cl[32];
    int numchars = 1;
    char method[8];
    char url[255];
    char path[512];
    char parm[2][32], parm_buf[32], *cgi_args[8];
    size_t i, j;
    struct stat st;
    int cgi = 0, par_k_cur = -1, par_v_cur = -1, par_cur = -1, cgi_arg_cur = 0;
    char *query_string = NULL;
    const char *server_root="./htdocs/";
//======================================================
    struct request req=parse_req(client);
    resp_msg(client, "200 ok", "echo", req.data);
    return;
//======================================================
    memset(parm,'\0', 2*32*sizeof(char) );

    // First line, the string before the first space, identifys the method of the request
    numchars = get_line(client, buf, sizeof(buf));
    fp=fopen("fnt.log","at+");
    fputs(buf,fp);
    fclose(fp);
    if ( (numchars<strlen("GET /\n"))||(strlen(buf)<strlen("GET /\n")) )
      {
        printf("<< Noise.\n");
        close(client);
        shutdown(client,2);
        return;
      }
    printf("=================================================================\n< %s", buf);

    i = 0; j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1) && (j < strlen(buf)))
    {
        method[i] = buf[j];
        i++; j++;
    }
    method[i] = '\0';

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
      {
        printf("! Unimplemented method: %s\n", method);
        discard_headers(client);
        resp_msg(client, "501 Unimplemented method", "501 error", "501 error<br>Bad Request");
        return;
      }

    while (ISspace(buf[j]) && (j < strlen(buf)))
        j++;

    i = 0;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < strlen(buf)))
    {
        if((buf[j] == '?') && (par_k_cur == -1) && (par_v_cur == -1))
          {
            memset(parm_buf, '\0', sizeof(parm_buf));
            par_k_cur = 0;
          }
        else if((buf[j] == '&') && ( par_k_cur == -1 ))
          {
            if ( (par_v_cur > -1) && (cgi_arg_cur > 0) && (par_cur < 0) && (strlen(parm_buf) > 0) )
              {
                cgi_args[cgi_arg_cur] = (char*)malloc(strlen(parm_buf));
                if (cgi_args[cgi_arg_cur] == NULL)
                  {
                    printf("! Failed_to_request_mem.\n");
                    discard_headers(client);
                    resp_msg(client, "501 Internal Error", "501 error", "501 error<br>The server seems in trouble now, please try again latter.");
                    return;
                  }
                strcpy(cgi_args[cgi_arg_cur], parm_buf);
              }
            memset(parm_buf, '\0', sizeof(parm_buf));
            par_k_cur = 0;
            par_v_cur = -1;
          }
        else if((buf[j] == '=') && (par_k_cur > -1) && ( par_v_cur == -1 ))
          {
            par_k_cur = -1;
            par_v_cur = 0;
            if (strcmp(parm_buf, "action") == 0 )
                 par_cur = REQ_PAR_ACTION;
            else if(strcmp(parm_buf, "id") == 0)
                 par_cur = REQ_PAR_ID;
            else if (strcmp(parm_buf, "carg") == 0)
              {
                cgi_arg_cur++;
                par_v_cur = 0;
                par_cur = -1;
                memset(parm_buf, '\0', sizeof(parm_buf));
              }
            else
              {
                 par_cur = -1;
              }
          }
        else if((par_k_cur > -1) && (par_k_cur < sizeof(parm_buf) - 1))
          {
            parm_buf[par_k_cur] = buf[j];
            par_k_cur++;
          }
        else if((par_v_cur > -1) && (par_cur > -1) && (par_v_cur < sizeof(parm[par_cur]) - 1))
          {
            parm[par_cur][par_v_cur] = buf[j];
            par_v_cur++;
          }
        else if((par_v_cur > -1) && (cgi_arg_cur > 0) && (par_cur < 0) && (par_v_cur < sizeof(parm_buf) - 1))
          {
            parm_buf[par_v_cur] = buf[j];
            par_v_cur++;
          }

        url[i] = buf[j];
        i++; j++;
    }

    if ( (par_v_cur > -1) && (cgi_arg_cur > -1) && (par_cur < 0) && (strlen(parm_buf) > 0) )
      {
        cgi_args[cgi_arg_cur] = (char*)malloc(strlen(parm_buf));
        strcpy(cgi_args[cgi_arg_cur], parm_buf);
      }
    //cgi_args[++cgi_arg_cur] = (char*)malloc();
    cgi_args[++cgi_arg_cur] = (char*)0;

    url[i] = '\0';
    query_string = url;
    while ((*query_string != '?') && (*query_string != '\0'))
        query_string++;
    if (*query_string == '?')
      {
        *query_string = '\0';
        query_string++;
      }

    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

    cgi_args[0] = (char*)malloc(strlen(parm[REQ_PAR_ACTION]));
    strcpy(cgi_args[0], parm[REQ_PAR_ACTION]);
    printf("# url: %s, par0: %s, path: %s\n", url, parm[0], path);
    for (par_v_cur=0; par_v_cur<cgi_arg_cur-1; par_v_cur++)
        printf("# cgi_arg: %s, par_v_cur:%d/%d\n", cgi_args[par_v_cur], par_v_cur, cgi_arg_cur-1);
  
    if (( !strcmp("/cgi.do", url) ) && strcasecmp(method, "GET")==0 )
      {
        execute_cgi(client, parm[REQ_PAR_ACTION], cgi_args, method, query_string);
        for (par_v_cur=0; par_v_cur< cgi_arg_cur; par_v_cur++)
          {
            free(cgi_args[par_v_cur]);
            cgi_args[par_v_cur] = (char*)0;
          }
        return;
      }
    else if (( !strcmp("/update_svc.do", url) ) && strcasecmp(method, "POST")==0)
      {
        handle_post_req(client);
        return;
      }

    if (stat(path, &st) == -1)
      { 
        not_found(client);
        return;
      }

    if ((st.st_mode & S_IFMT) == S_IFDIR)
          strcat(path, "/index.html");

    if (stat(path, &st) == -1)
        not_found(client);

    if ( (st.st_mode & 0100000) && (st.st_mode & 0400) )
      {
        serve_file(client, path);
        close(client);
        shutdown(client, 2);
        return;
      }
    else
    {
      if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)    )
          cgi = 1;
      if (!cgi) 
        {
          discard_headers(client);
          resp_msg(client, "200 OK", "Jerry", "Show Time!");
          return;
        }
    }

    close(client);
    shutdown(client, 2);
}

void discard_headers(int client)
  {
    char buf[128];
    int numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf))
      {
        printf("< %s", buf);
        numchars = get_line(client, buf, sizeof(buf));
      }
  }
/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    resp_msg(client, "400 BAD REQUEST", "BAD REQUEST", "Your browser sent a bad request, such as a POST without a Content-Length.");
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void cannot_execute(int client)
  {
    resp_msg(client, "500 Internal Server Error", "500 ERROR", "Error: prohibited CGI execution.");
  }

void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path, char* const *args, const char *method, const char *query_string)
{
    char buf[4096], buf_cl[32];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1, content_counter = 0;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)
        printf("# execute_cgi, get\n");
    else
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
          {
            printf("< %s\n", buf);
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
              { 
                content_length = atoi(&(buf[16]));
                printf("# content_length_of_request: %d\n", content_length);
              }
            numchars = get_line(client, buf, sizeof(buf));
          }
        discard_headers(client);
        if (content_length == -1)
          {
            bad_request(client);
            return;
          }

        for (i = 0; i < content_length && i < sizeof(buf)-2; i++)
             recv(client, buf+i, 1, 0);

        if (content_length <= sizeof(buf)-1)
            buf[content_length]='\0';
        else
          {
            buf[sizeof(buf)-1]='\0';
            buf[sizeof(buf)-2]='.';
            buf[sizeof(buf)-3]='.';
            buf[sizeof(buf)-4]='.';
          }

        printf("# post data: %s\n", buf);

    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    if ((pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        /* 把 STDOUT 重定向到 cgi_output 的写入端 */
        dup2(cgi_output[1], 1);
        /* 把 STDIN 重定向到 cgi_input 的读取端 */
        dup2(cgi_input[0], 0);
        /* 关闭 cgi_input 的写入端 和 cgi_output 的读取端 */
        close(cgi_output[0]);
        close(cgi_input[1]);
        /*设置 request_method 的环境变量*/
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            /*设置 query_string 的环境变量*/
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            /*设置 content_length 的环境变量*/
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        /*用 execl 运行 cgi 程序*/
        //char* targs[3] = {"ls", "/tmp/", (char*)0};
        execvp(path, args);
        exit(0);
    } else {    /* parent */
        /* 关闭 cgi_input 的读取端 和 cgi_output 的写入端 */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            /*接收 POST 过来的数据*/
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                /*把 POST 数据写入 cgi_input，现在重定向到 STDIN */
                write(cgi_input[1], &c, 1);
            }
        /*读取 cgi_output 的管道输出到客户端，该管道输入是 STDOUT */
        i = 0;
        while ((read(cgi_output[0], &c, 1) > 0) && i < sizeof(buf) - 1)
          {
            //send(client, &c, 1, 0);
            memcpy(buf+i, &c, 1);
            i++;
          }
        buf[i] = '\0';
        sprintf(buf_cl, "Content-Length: %d\r\n", (int)strlen(buf));
        send(client, buf_cl, strlen(buf_cl), 0);
        send(client, "\r\n", 2, 0);
        send(client, buf, strlen(buf), 0);
        /*关闭管道*/
        close(cgi_output[0]);
        close(cgi_input[1]);
        /*等待子进程*/
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    discard_headers(client);
    resp_msg(client, "404 NOT FOUND", "Not Found", "The server could not fulfill\r\n"
                     "your request because the resource specified\r\n"
                     "is unavailable or nonexistent.\r\n");
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[256];

    discard_headers(client);
    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
      {  
        fseek(resource, 0, SEEK_END);
        sprintf(buf, "HTTP/1.0 200 OK\r\n"
                     SERVER_STRING
                     "Content-Type: text/html\r\n"
                     "Content-Length: %ld\r\n\r\n", ftell(resource));
        fseek(resource, 0, SEEK_SET);;
        send(client, buf, strlen(buf), 0);
        cat(client, resource);
        fclose(resource);
    }
  close(client);
  shutdown(client, 2);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    // allocate one port dynamically if not specified.
    if (*port == 0)
    {
        int namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

void resp_msg(int client, char *status, char *title, char *msg)
  {
    if ((NULL==status)||(NULL==title)||(NULL==msg))
      {
        printf("! Invalid invocation of func resp_msg.");
        return;
      }
    int i, title_size_limit=16, n, msg_size_limit=412;
    char buf[512], buf_cl[32];
    if (strlen(status)>sizeof(buf_cl)-1)
      {
        for(i=0;i<sizeof(buf_cl)-4;i++)
            buf_cl[i]=*(status+i);
        for(i=sizeof(buf_cl)-4;i<sizeof(buf_cl)-1;i++)
            buf_cl[i]='.';
        buf_cl[sizeof(buf_cl)-1]='\0';
      }
    else
      strcpy(buf_cl, status);

    sprintf(buf, "HTTP/1.1 %s\r\n"
                SERVER_STRING
                "Content-Type: text/html\r\n", buf_cl);
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "<HTML><HEAD><TITLE>");
    n=strlen(buf);
    
    if (strlen(title)>title_size_limit)
      {
        for(i=0;i<title_size_limit-4;i++)
            buf[n+i]=*(title+i);
        for(i=title_size_limit-4;i<title_size_limit-1;i++)
            buf[n+i]='.';
        buf[n+title_size_limit-1]='\0';
      }
    else
        strcat(buf,title);

    strcat(buf,"</TITLE></HEAD>\r\n<BODY><P>");
    n=strlen(buf);

    if (strlen(msg)>msg_size_limit)
      {
        for(i=0;i<msg_size_limit-4;i++)
            buf[n+i]=*(msg+i);
        for(i=msg_size_limit-4;i<msg_size_limit-1;i++)
            buf[n+i]='.';
        buf[n+msg_size_limit-1]='\0';
      }
    else
        strcat(buf,msg);

    strcat(buf, "</P></BODY></HTML>\r\n");
    sprintf(buf_cl, "Content-Length: %ld\r\n\r\n", strlen(buf));
    send(client, buf_cl, strlen(buf_cl), 0);
    send(client, buf, strlen(buf), 0);
    close(client);
    shutdown(client, 2);
  }

void handle_post_req(int client)
  {
    char buf[4096], buf_cl[32], c;
    int status, i, numchars = 1;
    int content_length = -1, content_counter = 0;
    
    buf[0] = 'A'; buf[1] = '\0';
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf) && (status<32))
      {
        printf("< %s", buf);
        numchars = strlen(buf);
        i = numchars-1;
        while ( ( ( buf[i] == '\n' ) || ( isspace((int)(buf[i])) ) ) && (i >= 0) )
          {
            buf[i] = '\0';
            i--;
          }
        i = 0;
        while( (buf[i] != ':') && (i<numchars) )
            i++;

        if (i<numchars)
            buf[i] = '\0';

        while (isspace((int)(buf[++i])) && (i<numchars) )
            continue;

        if (strcasecmp(buf, "Content-Length") == 0)
          {

            if ( i < numchars )
              {
                content_length = atoi(&(buf[i]));
              }
          }
        else if (strcasecmp(buf, "Connection") == 0)
          {
            if ( ( i < numchars ) && ( strcasecmp(buf+i, "keep-alive") == 0) )
                printf("# One keep-alive session.\n");
          }
        numchars = get_line(client, buf, sizeof(buf));
      }
    if (content_length == -1)
      {
        bad_request(client);
        return;
      }
    for (i = 0; i < content_length && i < sizeof(buf)-2; i++)
        recv(client, buf+i, 1, 0);

    if (content_length <= sizeof(buf)-1)
        buf[content_length]='\0';
    else
      {
        buf[sizeof(buf)-1]='\0';
        buf[sizeof(buf)-2]='.';
        buf[sizeof(buf)-3]='.';
        buf[sizeof(buf)-4]='.';
      }
    printf("< DATA: %s\n", buf);
    resp_msg(client, "200 OK", "Fullfilled", "The request has been fullfilled");
    return;
  }

int main(void)
{
    int server_sock = -1;
    u_short port = 60080;
    int client_sock = -1;
    pid_t fpid;
    struct sockaddr_in client_name;
    int client_name_len = sizeof(client_name);
    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        client_sock = accept(server_sock,(struct sockaddr *)&client_name,&client_name_len);
        if (client_sock == -1)
          {
            printf("! Got one accept_failure.\n");
            continue;
          }
        fpid = fork();
        if (fpid == 0)
          {
            accept_request(client_sock);
          }
    }

    close(server_sock);

    return(0);
}
