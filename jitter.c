#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<math.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>

/*struct sockaddr_in *init()
{
	struct sockaddr_in *sos=malloc(sizeof(struct sockaddr_in));
	sos->sin_port=htons(46359);
	sos->sin_family=AF_INET;
	inet_pton(AF_INET,"127.0.0.1",(struct sockaddr *)sos);
	return sos;
}*/

void main()
{
	int sock=socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in sos;
	sos.sin_port=htons(6796);
	sos.sin_family=AF_INET;
	inet_pton(AF_INET,"45.77.61.22",(struct sockaddr *)&sos.sin_addr.s_addr);
	connect(sock,(struct sockaddr *)&sos,sizeof(sos));
	struct timespec start, end;
	int buffer=0;
	double l[10],j=0;
	for(int i=0;i<10;i++)
	{
		clock_gettime(CLOCK_REALTIME, &start);
		send(sock,&i,sizeof(int),0); 
		recv(sock,&buffer,sizeof(int),0); 
		clock_gettime(CLOCK_REALTIME, &end);
		l[i]=(((double)end.tv_nsec - (double)start.tv_nsec)/1000000)+((double)end.tv_sec-(double)start.tv_sec)*1000;
		printf(">\t[%d] %.4lfms\n", i, l[i]);
	}
	for(int i=0;i<9;i++)
	{
		j+=(l[i+1]-l[i]<=0) ? (l[i+1]-l[i])*(-1) : (l[i+1]-l[i]);
	}
	j=j/5;
	printf("\n>\tjitter=%.4lfms\n", j);
}
