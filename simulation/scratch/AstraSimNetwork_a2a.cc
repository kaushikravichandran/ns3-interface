#include "ns3/AstraNetworkAPI.hh"
#include<iostream>
#include <stdio.h>
#include <execinfo.h>
#include <queue>
#include <string>
#include <thread>
#include <unistd.h>
#include "workerQueue.h"
#include "third.cc"
#include <vector>
#include <map>
// #include "myTCPMultiple.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/Sys.hh"
#include <time.h> 
// #include "RoCE.h"
//#include<type_info>
using namespace std;
using namespace ns3;
//NS_LOG_COMPONENT_DEFINE ("ASTRASimNetwork");
// struct sim_comm {
//   std::string comm_name;
// };
// enum time_type_e { "SE" };

// struct timespec_t {
//   time_type_e time_res;
//   double time_val;
// };
// extern int global_variable;
std::vector<int> physical_dims{8};
queue<struct task1> workerQueue;
unsigned long long tempcnt = 999;
unsigned long long  cnt = 0;
int num_gpus=1;
class ASTRASimNetwork;
std::vector<ASTRASimNetwork*> networks;
int collective_counter=0;
struct sim_event {
    void* buffer;
    uint64_t count;
    int type;
    int dst;
    int tag;
    // AstraSim::sim_request* request;
    // void (*msg_handler)(void* fun_arg);
    // void* fun_arg;
    string fnType;
};
Ptr<SimpleUdpApplication> *udp;
class ASTRASimNetwork:public AstraSim::AstraNetworkAPI{

    public:
        queue<sim_event> sim_event_queue;
        ASTRASimNetwork(int rank):AstraNetworkAPI(rank){
            //cout<<"hello constructor\n";
            // workerQueue.push("avg");
        }
        ~ASTRASimNetwork(){}
        int sim_comm_size(AstraSim::sim_comm comm, int* size){
            return 0;
        }
        int sim_finish(){
            //cout<<"sim finish\n";
            // Simulator::Destroy ();
            for(auto it = nodeHash.begin();it!=nodeHash.end();it++){
                pair<int,int> p = it->first;
                if(p.second==0){
                    cout<<"All data sent from node "<<p.first<<" is "<<it->second<<"\n";
                }
                else{
                    cout<<"All data received by node "<<p.first<<" is "<<it->second<<"\n";
                }
            }
            return 0;
        }
        double sim_time_resolution(){
            return 0;
        }
        int sim_init(AstraSim::AstraMemoryAPI* MEM){
            return 0;
        }
        AstraSim::timespec_t sim_get_time(){
            AstraSim::timespec_t timeSpec;
            // timeSpec.time_type_e = "SE";
            timeSpec.time_val = Simulator::Now().GetNanoSeconds();
            return timeSpec;
        }
        virtual void sim_schedule(
            AstraSim::timespec_t delta,
            void (*fun_ptr)(void* fun_arg),
            void* fun_arg){
                //delta.time_val = 5; //trial
                task1 t;
                t.type = 2;
                t.fun_arg = fun_arg;
                t.msg_handler = fun_ptr;
                t.schTime = delta.time_val;
                // workerQueue.push(t);
                Simulator::Schedule (NanoSeconds (t.schTime), t.msg_handler, t.fun_arg);
                //cout<<"sim schedule is called "<<endl;
                return;
            }
        virtual int sim_send(
            void* buffer, //not yet used 
            uint64_t count, //number of bytes to be send
            int type,//not yet used 
            int dst,
            int tag, //not yet used 
            AstraSim::sim_request* request,//not yet used 
            void (*msg_handler)(void* fun_arg),
            void* fun_arg){
		if(rank==0 && dst == 1 && cnt == 0){
		cout<<"lets go \n";
		}
                // int src = 0;
                //populate task1 with the required arguments
                task1 t;
                t.src = rank; //how to get src, is it the rank (starts from 0?)
                t.dest = dst;
                // int a = 1;
                t.count = count; 
                //t.fun_arg = &a;
                t.type = 0;
                t.fun_arg = fun_arg;
                t.msg_handler = msg_handler;
                // workerQueue.push(t); 
                // udp[t.src]->SendPacket(t.dest, t.fun_arg, t.msg_handler, t.count, tag);
		//cout<<"COUNT and PACKET is "<<count<<" "<<maxPacketCount<<"\n";
                //static int totalSends=0;
                //totalSends++;
                //cout<<"total sends: "<<totalSends<<" src dest count "<<rank<<" "<<dst<<" "<<count<<endl;
		//cout<<"src dst cOUNT IN SEND IS "<<rank<<" "<<dst<<" "<<count<<"\n";
		sentHash[make_pair(tag,make_pair(t.src,t.dest))] = t;
                SendFlow(rank, dst , count, msg_handler, fun_arg,tag);
	        //cout<<"event at sender pushed "<<t.src<<" "<<" "<<t.dest<<" "<<tag<<"\n";
                return 0;
            }
        virtual int sim_recv(
            void* buffer,
            uint64_t count,
            int type,
            int src,
            int tag,
            AstraSim::sim_request* request,
            void (*msg_handler)(void* fun_arg),
            void* fun_arg){
                //populate task1 with the required arguments
                task1 t;
                t.src = src;
                t.dest = rank; //how to get dest, is it the rank (starts from 0?)
                // int a = 1;
                t.count = count; 
                //t.fun_arg = &a;
                t.type = 1;
                t.fun_arg = fun_arg;
                t.msg_handler = msg_handler;
                // workerQueue.push(t);
                if(recvHash.find(make_pair(tag,make_pair(t.src,t.dest)))!=recvHash.end()){
                    int count = recvHash[make_pair(tag,make_pair(t.src,t.dest))];
                    if(count == t.count)
                    {
                        recvHash.erase(make_pair(tag,make_pair(t.src,t.dest)));
                        //cout<<"already in recv hash "<<t.src<<" "<<t.dest<<" "<<tag<<"\n";
                        t.msg_handler(t.fun_arg);
                    }
                    else if (count > t.count){
                        recvHash[make_pair(tag,make_pair(t.src,t.dest))] = count - t.count;
                        //cout<<"already in recv hash with more data "<<t.src<<" "<<t.dest<<" "<<tag<<"\n";
                        t.msg_handler(t.fun_arg);
                    }
                    else{
                        recvHash.erase(make_pair(tag,make_pair(t.src,t.dest)));
                        t.count -= count;
                        expeRecvHash[make_pair(tag,make_pair(t.src,t.dest))] = t;
                        //cout<<"partially in recv hash "<<t.src<<" "<<t.dest<<" "<<tag<<"\n";
                    }
                }
                else{
		    if(expeRecvHash.find(make_pair(tag,make_pair(t.src,t.dest)))==expeRecvHash.end()){
                        expeRecvHash[make_pair(tag,make_pair(t.src,t.dest))] = t;
                        //cout<<"not in recv hash "<<t.src<<" "<<t.dest<<"  "<<tag<<"\n";
                    }
                    else{
                        int expecount = expeRecvHash[make_pair(tag,make_pair(t.src,t.dest))].count;
                        t.count += expecount;
                        expeRecvHash[make_pair(tag,make_pair(t.src,t.dest))] = t;
                        //cout<<"not in recv hash but in expected recv hash "<<t.src<<" "<<t.dest<<" "<<tag<<"\n";
                    }
                    //expeRecvHash[make_pair(tag,make_pair(t.src,t.dest))] = t;
                }
		//cout<<"COUNT IN RECV IS "<<count<<"\n";
                //cout<<"event at receiver pushed\n";
                return 0;
            }
        void  handleEvent(int dst,int cnt) {
            //cout<<"test\n";
            //cout<<"event pushed\n";
        }
};

void fun_send(void* a) {
    //cout<<*(int *)a<<"Having fun in send!"<<"\n";
}
void fun_recv(void* a) {
	//unsigned long long cnt = 0;
	tempcnt++;cnt++;
	if(tempcnt==1000){
		tempcnt = 1;
		auto now = chrono::system_clock::now();
auto now_c = std::chrono::system_clock::to_time_t(now);
std::cout << cnt <<"at time "<<std::ctime(&now_c)<<"\n";//std::put_time(std::localtime(&now_c), "%c") << '\n';
		//cout<<"send times is "<<cnt<<"\n";
	}
	int fun_arg=1;
    ASTRASimNetwork network0 = ASTRASimNetwork(0);
    ASTRASimNetwork network1 = ASTRASimNetwork(1);
    task1 t;
    t.src = 0;
    t.dest = 1;
    t.count = 3000;
    t.type = 1;
    t.fun_arg = &fun_arg;
    t.msg_handler= &fun_recv;
    int tag = 100;
    expeRecvHash[make_pair(tag,make_pair(t.src,t.dest))] = t;
    t.src = 1; t.dest = 0;
    expeRecvHash[make_pair(tag,make_pair(t.src,t.dest))] = t;
    network0.sim_send(nullptr,3000,-1,1,100,nullptr,&fun_send,&fun_arg);
    network1.sim_send(nullptr,3000,-1,0,100,nullptr,&fun_send,&fun_arg);
	//cout<<*(int *)a<<"Having fun in recv!"<<"\n";
}
void fun_sch(void* a) {
    //cout<<*(int *)a<<"Having fun in schedule!"<<"\n";
}
Ptr<SimpleUdpApplication>* sim_init(int n){
    //cout<<"sim init is called"<<endl;
    NodeContainer nodes;
    nodes.Create (n);
    CsmaHelper csma;  
    csma.SetChannelAttribute ("DataRate", StringValue ("1Gbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds(6560)));

    NetDeviceContainer csmaDevs;
    csmaDevs = csma.Install (nodes);
    csma.EnableAsciiAll("simple-udp");

    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces;
    ifaces = address.Assign (csmaDevs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    Packet::EnablePrinting (); 

    //Create Two UDP applications
    // //Set the start & stop times
    //Ptr<SimpleUdpApplication> udp[n];
    udp = new Ptr<SimpleUdpApplication>[n];
    //assumes astra sim pass from index 0;
    for(int i = 0;i<n;i++){
        udp[i] = CreateObject <SimpleUdpApplication> ();
        udp[i]->SetStartTime(Seconds(0));
        udp[i]->SetStopTime(Seconds(100));
        // udp[i]->InitializeAppRecv(n,i);    
        nodes.Get(i)->AddApplication(udp[i]);    
    }
    for(int i = 0;i<n;i++){      
        udp[i]->InitializeAppRecv(n,i); 
    }
    for(int i = 0;i<n;i++){
        udp[i]->InitializeAppSend(n,i,ifaces);    
        
    }
    return udp;
    // Ipv4Address dest_ip = ifaces.GetAddress(1);
    // //cout<<"dest ip is "<<dest_ip<<"\n";
    // while(!workerQueue.empty()){
    //     task1 t1 = workerQueue.front();
    //     //cout<<"woker queue with task id: "<<t1.type<<" is scheduled"<<endl;
    //     if(t1.type==0){
    //         Ipv4Address dest_ip1 = ifaces.GetAddress(t1.dest);
    //         //cout<<"dest ip is "<<dest_ip1<<"\n";
    //         udp[t1.src]->SendPacket(t1.dest, t1.fun_arg, t1.msg_handler, t1.count);
    //         // Simulator::Schedule (Seconds (0), &SimpleUdpApplication::SendPacket, udp[t1.src], t1.dest, t1.fun_arg, t1.msg_handler, t1.count);
    //     }
    //     else if(t1.type==1){
    //         //WHENEVER SENDING TO RECEIVE FUNCTION first check if that pair already in receive hash. 
    //         if(recvHash.find(make_pair(t1.src,t1.dest))!=recvHash.end()){
    //             int count = recvHash[make_pair(t1.src,t1.dest)];
    //             if(count == t1.count)
    //             {
    //                 recvHash.erase(make_pair(t1.src,t1.dest));
    //                 t1.msg_handler(t1.fun_arg);
    //                 //cout<<"already in recv hash\n";
    //             }
    //             else if (count > t1.count){
    //                 recvHash[make_pair(t1.src,t1.dest)] = count - t1.count;
    //                 t1.msg_handler(t1.fun_arg);
    //                 //cout<<"already in recv hash with more data\n";
    //             }
    //             else{
    //                 recvHash.erase(make_pair(t1.src,t1.dest));
    //                 t1.count -= count;
    //                 expeRecvHash[make_pair(t1.src,t1.dest)] = t1;
    //                 //cout<<"partially in recv hash\n";
    //             }
    //         }
    //         else{
    //             expeRecvHash[make_pair(t1.src,t1.dest)] = t1;
    //             //cout<<"not in recv hash\n";
    //         }
    //     }
    //     else{
    //         Simulator::Schedule (NanoSeconds (t1.schTime), t1.msg_handler, t1.fun_arg);
    //     }
    //     workerQueue.pop();
    // }
//    Simulator::Schedule (Seconds (4), &SimpleUdpApplication::FinishTask, udp[t.src]); 
//    Simulator::Schedule (Seconds (5), &SimpleUdpApplication::processQueue, udp[t.src]);
    // Ptr <Packet> packet2 = Create <Packet> (800);
    // LogComponentEnable ("SimpleUdpApplication", LOG_LEVEL_INFO);
    // Simulator::Schedule (Seconds (2), &SimpleUdpApplication::SendPacket, udp[0], packet2, dest_ip, 9999);

    // Simulator::Run ();
    
    // Simulator::Stop (Seconds (100));
}

class A2AUnoptimized;
void dummy_msg_handler(void *arg);
void send_handler(void *arg);

class HandlerData{
    public:
        A2AUnoptimized *collective;
        int gpu_id;
        HandlerData(A2AUnoptimized *collective,int gpu_id){
            this->gpu_id=gpu_id;
            this->collective=collective;
        }
};
class A2AUnoptimized{
    public:
        int collective_comm_id;
        std::vector<ASTRASimNetwork*> *networks;
        std::map<int,int> A2A_counter;
        int start_GPU;
        int end_GPU;
        uint64_t msg_size;
        A2AUnoptimized(std::vector<ASTRASimNetwork*> *networks,int start_GPU,int end_GPU,uint64_t size){
            this->networks=networks;
            this->start_GPU=start_GPU;
            this->end_GPU=end_GPU;
            this->msg_size=size/(end_GPU-start_GPU);
            for(int i=start_GPU;i<=end_GPU;i++){
                A2A_counter[i]=start_GPU;
            }
            A2A_counter[start_GPU]++;
            collective_comm_id=collective_counter++;
        }
        void execute(){
            for(int i=start_GPU;i<=end_GPU;i++){
                for(int j=start_GPU;j<=end_GPU;j++){
                    if(i!=j){
                        (*networks)[i]->sim_recv(nullptr,
                                              msg_size,
                                              0,
                                              j,
                                              collective_comm_id,
                                              nullptr,
                                              &dummy_msg_handler,
                                              nullptr);
                    }
                }
            }
            for(int i=start_GPU;i<=end_GPU;i++){
                HandlerData *handlerData=new HandlerData(this,i);
                (*networks)[i]->sim_send(nullptr, //not yet used
                                      msg_size, //number of bytes to be send
                                      0,//not yet used
                                      A2A_counter[i],
                                      collective_comm_id, //not yet used
                                      nullptr,//not yet used
                                      &send_handler,
                                      handlerData);
            }
        }
        void send_finished(int gpu_id){
            A2A_counter[gpu_id]++;
            if(A2A_counter[gpu_id]==gpu_id){
                A2A_counter[gpu_id]++;
            }
            if(A2A_counter[gpu_id]<=end_GPU){
                HandlerData *handlerData=new HandlerData(this,gpu_id);
                (*networks)[gpu_id]->sim_send(nullptr, //not yet used
                                         msg_size, //number of bytes to be send
                                         0,//not yet used
                                         A2A_counter[gpu_id],
                                         collective_comm_id, //not yet used
                                         nullptr,//not yet used
                                         &send_handler,
                                         handlerData);
            }
        }

};
void dummy_msg_handler(void *arg){

}
void send_handler(void *arg){
    HandlerData * handler_data=(HandlerData*)arg;
    handler_data->collective->send_finished(handler_data->gpu_id);
    delete handler_data;
}

int main (int argc, char *argv[]){
    for(auto &a:physical_dims){
        num_gpus*=a;
    }
    std::string system_input;
    if(physical_dims.size()==1){
        system_input="sample_a2a_sys.txt";
    }
    else if(physical_dims.size()==2){
        system_input="sample_2D_switch_sys.txt";
    }
    else if(physical_dims.size()==3){
        system_input="sample_3D_switch_sys.txt";
    }
    LogComponentEnable ("SimpleUdpApplication", LOG_LEVEL_INFO);
    //cout << "Hello world!\n";
    // LogComponentEnable("myTCPMultiple",LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    //ASTRASimNetwork network0 = ASTRASimNetwork(255);
    //ASTRASimNetwork network1 = ASTRASimNetwork(254);
    networks.resize(num_gpus,nullptr);
    //std::vector<AstraSim::Sys*> systems(num_gpus,nullptr);
    //std::vector<int> physical_dims(1,num_gpus);
    std::vector<int> queues_per_dim(physical_dims.size(),1);
    for(int i=0;i<num_gpus;i++){
	networks[i]=new ASTRASimNetwork(i);	
	/*systems[i] = new AstraSim::Sys(
        	networks[i], // AstraNetworkAPI
        	nullptr, // AstraMemoryAPI
        	i, // id
        	1, // num_passes
        	physical_dims, // dimensions
        	queues_per_dim, // queues per corresponding dimension
        	"../astra-sim/inputs/system/"+system_input, // system configuration
        	"../astra-sim/inputs/workload/microAllReduce.txt", //DLRM_HybridParallel.txt, // Resnet50_DataParallel.txt, // workload configuration
        	256, // communication scale
        	1, // computation scale
        	1, // injection scale
        	1,
        	0, // total_stat_rows and stat_row
        	"scratch/results/", // stat file path
        	"test1", // run name
        	true, // separate_log
        	false  // randezvous protocol
    	);*/
    }	
    //int fun_arg=1;
    main1(argc, argv);
    //network0.sim_send(nullptr,3000,-1,1,100,nullptr,&fun_send,&fun_arg);
    //network1.sim_send(nullptr,3000,-1,1,100,nullptr,&fun_recv,&fun_arg);
    //network.sim_schedule(AstraSim::timespec_t(),&fun_sch,&fun_arg);
    //pass number of nodes
    // Ptr<SimpleUdpApplication> *udp = sim_init(num_gpus);
    //fun_recv(&fun_arg);
    /*for(int i=0;i<num_gpus;i++){
	systems[i]->workload->fire();	
    }*/
    auto now = chrono::high_resolution_clock::now();
    auto now_c = std::chrono::high_resolution_clock::to_time_t(now);
    std::cout<<"at time "<<now_c<<"\n"; 
    //int64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    //std::cout<<"timestamp "<<timestamp<<"\n";
    //auto start = std::chrono::high_resolution_clock::now();
    //std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(start).count() << "ns\n";
    std::cout<<"nano sec "<<Simulator::Now().GetNanoSeconds()<<"\n";

    //sample generation of A2A collectives: A2AUnoptimized(reference to network instances,start_gpu_id,end_gpu_id,A2A_size)
    A2AUnoptimized collective1(&networks,0,3,1024*1024);
    A2AUnoptimized collective2(&networks,4,7,1024*1024);
    collective1.execute();
    collective2.execute();

    Simulator::Run ();
    //Simulator::Stop(TimeStep (0x7fffffffffffffffLL)); 
    auto now1 = chrono::system_clock::now();
    auto now_c1 = std::chrono::system_clock::to_time_t(now1);
    std::cout<<"end time "<<now_c1<<"\n"; //std::ctime(&now_c1)<<"\n";
    Simulator::Stop(Seconds (2000000000));
    Simulator::Destroy();
    return 0;
}
