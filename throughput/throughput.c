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

void print_data(message m)
{
	printf("\n\n\n\n\n");
	printf("temp=%.2f\n",m.temp);
	printf("hum=%.2f\n",m.hum);
	printf("bat=%.2f\n", m.bat);
	printf("bat=%.2f\n",m.roll);
	printf("pitch=%.2f\n",m.pitch);
	printf("waw=%.2f\n",m.waw);
	printf("alt=%.2f\n",m.alt);
	for(int i=0;i<3;i++)
		printf("pmw[%d] = %d\n", i, m.pmw[i]);
	printf("\n\n\n\n");
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
	char *unit=malloc(sizeof(char)*2); //throughput unit
	int port;
	char addr[15];
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
	struct sockaddr_in sos;
	int sock=socket(AF_INET,SOCK_STREAM,0);
	FILE *mt = fopen("/sys/class/net/eth0/mtu","r");//unix device mtu size
	double downthroughput[10], upthroughput[10]; //throughput vector for average throughput measurement


	if(argc>0)
	{
		if(argc==5)
		{
			for(int i=1;i<argc;i++)
			{
				if(strcmp(argv[i],"-p")==0)
				{
					i++;
					port=atoi(argv[i++]);
				}
				if(strcmp(argv[i],"-a")==0)
				{
					i++;
					strcpy(addr,argv[i++]);
				}
			}
		}
	}
	fscanf(mt,"%d", &mtu);
	fclose(mt);
	printf("mtu=%d\n",mtu);
	sos.sin_port=htons(port);
	sos.sin_family=AF_INET;
	inet_pton(AF_INET,addr,(struct sockaddr *)&sos.sin_addr.s_addr);
	connect(sock,(struct sockaddr *)&sos,sizeof(sos));
	struct timespec start, end;
	dummy_initializer(&data);
	memset(send_buffer,0,sizeof(send_buffer));
	copy_struct(temp,data);
	for(int i=0;i<10;i++)
	{
		clock_gettime(CLOCK_REALTIME, &start);
		sendbytes=send(sock,send_buffer,sizeof(message),0); 
		recvbytes=recv(sock,&data,sizeof(message),0); 
		clock_gettime(CLOCK_REALTIME, &end);
		l[i]=(((double)end.tv_nsec - (double)start.tv_nsec)/1000000)+((double)end.tv_sec-(double)start.tv_sec)*1000;
		printf(">\t[%d] %.4lfms\n", i, l[i]);
		downthroughput[i]=throughput_m(load(packet_number(mtu,recvbytes),mtu), l[i], &down_tp);
		upthroughput[i]=throughput_m(load(packet_number(mtu,sendbytes),mtu), l[i], &up_tp);
		unitchooser(unit,downthroughput[i]);
		printf("download %.2lf/%ss\n", unitchooser(unit, downthroughput[i]), unit);
		unitchooser(unit,upthroughput[i]);
		printf("upload %.2lf/%ss\n", unitchooser(unit,upthroughput[i]), unit);
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
