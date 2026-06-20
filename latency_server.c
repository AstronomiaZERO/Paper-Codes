#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<math.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>


void main()
{
	restart:
	int sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	struct sockaddr_in sos;
	sos.sin_port=htons(6796);
	sos.sin_family=AF_INET;
	sos.sin_addr.s_addr=INADDR_ANY;
	int bs = bind(sock, (struct sockaddr *)&sos, sizeof(sos));
	if(bs<0)
	{
		printf("error, reinitializing\n");
		goto restart;
	}
	while(true)
	{
		listen(sock, 1);
		int client=accept(sock, NULL, NULL);
		struct timespec start, end;
		int buffer=0;
		double l[10],j=0;
		for(int i=0;i<10;i++)
		{
			clock_gettime(CLOCK_REALTIME, &start);
			recv(client,&buffer,sizeof(int),0); 
			send(client,&buffer,sizeof(int),0); 
			clock_gettime(CLOCK_REALTIME, &end);
			l[i]=(((double)end.tv_nsec - (double)start.tv_nsec)/1000000)+((double)end.tv_sec-(double)start.tv_sec)*1000;
			printf(">\t[%d] %.4lfms\n", i, l[i]);
		}
		for(int i=0;i<9;i++)
		{
			j=(l[i+1]-l[i]>=0) ? j+(l[i+1]-l[i]) : j+((l[i+i]-l[i])*(-1));
		}
		j=j/5;
		printf("\n>\tjitter=%.4lfms\n", j);
	}
}
