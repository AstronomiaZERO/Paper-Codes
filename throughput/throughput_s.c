#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<math.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/tcp.h>
#include<netinet/in.h>	
#include<arpa/inet.h>
#include<pcap/pcap.h>


#define overhead 40 //no additional headers were used, so assume 40B of IPv4 overhead
#define MSS(x) ((x) - overhead)


typedef struct message
{
	float temp;
	float hum; //humidity
	float bat;
	float roll;
	float pitch;
	float waw;
	float alt; // altitude
	int pmw[4];
}message;

typedef struct through_data
{
	double max;
	double avg;
	double min;
}through_data;

void copy_struct(message *m, message original) //copies struct attributes because normal atribution doesnt initialize properly
{
	m->temp=original.temp;
	m->hum=original.hum;
	m->bat=original.bat;
	m->roll=original.roll;
	m->pitch=original.pitch;
	m->waw=original.waw;
	m->alt=original.alt;
	for(int i=0;i<3;i++)
		m->pmw[i]=original.pmw[i];
}


int packet_number(int mtu,int bytes)
{
	return ceil(bytes/(double)(MSS(mtu)));
}

int load(int np, int mtu)
{
	return np*mtu;
}

double throughput_m(int ld, double latency, through_data *tp)
{
	double l_sec=latency/1000; //latency converted to seconds
	double throughput = ld/l_sec;
	if(throughput>tp->max)
		tp->max=throughput;
	else
		if(throughput<tp->min)
			tp->min=throughput;
	return throughput;
}

double unitchooser(char *unit,double throughput)
{
	if(throughput>=1000)
	{
		strcpy(unit,"kb");
		throughput/=1000;
		if(throughput>=1000)
		{
			strcpy(unit,"mb");
			return throughput/1000;
		}
		return throughput;
	}
	else
	{
		strcpy(unit,"b");
		return throughput;
	}
}

void dummy_initializer(message *m)
{
	m->temp=67.2;
	m->hum=36.6424;
	m->bat=69.7;
	m->roll=3542.3;
	m->pitch=34.4;
	m->waw=34.4;
	m->alt=14.2;
	for(int i=0;i<3;i++)
		m->pmw[i]=i;
}
	

void main(int argc, char *argv[])
{
	int port, lnumber;
	char *unit=malloc(sizeof(char)*2); //throughput unit
	int mtu; //maximum transmission unit (varies per interface)
	int recvbytes=0, sendbytes=0; //received bytes
	through_data down_tp, up_tp; 
	up_tp.max=up_tp.avg=down_tp.max=down_tp.avg=0;
	down_tp.min=up_tp.min=999999999999999999;
	message data; //buffer to be sent/received
	//send doesnt send structures propperly, so we need to convert the struct to a string
	char send_buffer[sizeof(message)]; 
	message *temp=(message *)send_buffer; 
	double l[10]; //latency vector
	restart:
	struct sockaddr_in sos;
	int sock=socket(AF_INET,SOCK_STREAM,0);
	FILE *mt = fopen("/sys/class/net/eth0/mtu","r");//unix device mtu size
	double downthroughput[10], upthroughput[10]; //throughput vector for average throughput measurement

	if(argc>0)
	{
		if(argc==5)
		{
			for(int i=0;i<argc;i++)
			{
				if(strcmp(argv[i],"-p")==0) //port
				{
					i++;
					port=atoi(argv[i++]);
				}
				if(strcmp(argv[i],"-l")==0) 
				{
					i++;
					lnumber=atoi(argv[i++]); //number of devices to listen
				}
			}
		}
	}
	fscanf(mt,"%d", &mtu);
	fclose(mt);
	printf("mtu=%d\n",mtu);
	fscanf(mt,"%d", &mtu);
	fclose(mt);
	sos.sin_port=htons(port);
	sos.sin_family=AF_INET;
	sos.sin_addr.s_addr=INADDR_ANY;
	int bs = bind(sock, (struct sockaddr *)&sos, sizeof(sos));
	if(bs<0)
	{
		printf("error, reinitializing\n");
		goto restart;
	}
	listen(sock,lnumber);
	int client=accept(sock, NULL, NULL);
	struct timespec start, end;
	dummy_initializer(&data);
	memset(send_buffer,0,sizeof(send_buffer));
	copy_struct(temp,data);
	for(int i=0;i<10;i++)
	{
		clock_gettime(CLOCK_REALTIME, &start);
		recvbytes=recv(client,&data,sizeof(message),0); 
		sendbytes=send(client,send_buffer,sizeof(message),0); 
		clock_gettime(CLOCK_REALTIME, &end);
		l[i]=(((double)end.tv_nsec - (double)start.tv_nsec)/1000000)+((double)end.tv_sec-(double)start.tv_sec)*1000;
		printf(">\t[%d] %.4lfms\n", i, l[i]);
		downthroughput[i]=throughput_m(load(packet_number(mtu,recvbytes),mtu), l[i], &down_tp);
		upthroughput[i]=throughput_m(load(packet_number(mtu,sendbytes),mtu), l[i], &up_tp);
		unitchooser(unit,downthroughput[i]);
		printf("download %.2lf/%ss\n", downthroughput[i], unit);
		unitchooser(unit,upthroughput[i]);
		printf("upload %.2lf/%ss\n", upthroughput[i], unit);
	}
	double avg_ls=0; //average latency converted to seconds
	for(int i=0;i<10;i++)
	{
		avg_ls+=l[i];
	}
	avg_ls/=10; //calculates the average latency
	avg_ls/=1000; //converts from milliseconds to seconds
	for(int i=0;i<10;i++)
	{
		up_tp.avg+=upthroughput[i];
		down_tp.avg+=downthroughput[i];
	}
	up_tp.avg/=10;
	down_tp.avg/=10;
	up_tp.avg=unitchooser(unit,up_tp.avg);
	down_tp.avg=unitchooser(unit,up_tp.avg);
	up_tp.avg/=avg_ls;
	down_tp.avg/=avg_ls;
	printf("\n\naverage download %.2lf/%ss\t", down_tp.avg,unit);
	unitchooser(unit,down_tp.min);
	printf("minimum download %.2lf/%ss\t", unitchooser(unit, down_tp.min), unit);
	unitchooser(unit,down_tp.max);
	printf("maximum download %.2lf/%ss\n", unitchooser(unit, down_tp.max),unit);

	printf("\n\naverage upload %.2lf/%ss\t", unit);
	unitchooser(unit,down_tp.min);
	printf("minimum upload %.2lf/%ss\t", unitchooser(unit, up_tp.min), unit);
	unitchooser(unit,up_tp.max);
	printf(" maximum upload %.2lf/%ss\n", unitchooser(unit, up_tp.max),unit);
}
