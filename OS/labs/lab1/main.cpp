//
// Created by liuke on 2019/11/10.
//

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
//最多创建1000个小车线程
#define MAX 1000
//初始化信号量
//等待队列的信号量
pthread_mutex_t WaitEast;
pthread_mutex_t WaitWest;
pthread_mutex_t WaitNorth;
pthread_mutex_t WaitSouth;
//四个路口的信号量
pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;
pthread_mutex_t mutex_c;
pthread_mutex_t mutex_d;
//第一个路口的信号量
pthread_mutex_t block_e;
pthread_mutex_t block_w;
pthread_mutex_t block_n;
pthread_mutex_t block_s;
//第一个路口的条件变量
pthread_cond_t cond_w;
pthread_cond_t cond_e;
pthread_cond_t cond_n;
pthread_cond_t cond_s;
//等待队列的条件变量
pthread_cond_t FirstInEast;
pthread_cond_t FirstInWest;
pthread_cond_t FirstInSouth;
pthread_cond_t FirstInNorth;

//死锁检测线程信号量
pthread_mutex_t mutex_e;
//唤醒死锁线程的条件变量
pthread_cond_t cond_deadlock;
//死锁发生后，唤醒最后一个车左边一个车的条件变量
pthread_cond_t cond_unlock;
//记录最后一个进入车的方向
typedef enum { north, south, east, west }LastDir;
LastDir dir;

int size= 0;//记录车的数量
int empty;//记录空闲的路口数
//表示当前方向车辆的id
int SouthId;
int EastId;
int NorthId;
int WestId;

//保存所有的线程
pthread_t car[MAX];

//创建队列，用于保存每个方向创建的线程
struct queue
{
    pthread_t thread[MAX];//标识车的线程
    int num[MAX];//车的id
    int front;//front pointer
    int rear;//rear pointer
    int count;//该队列中车的数量
    queue() {
        front = rear = count = 0;
    }
    //入队
    void push(int n) {
        count++;
        rear = (rear + 1) % MAX;
        num[rear] = n;
    }
    //出对
    int pop() {
        count--;
        front = (front + 1) % MAX;
        return num[front];
    }
};
//创建四个方向的队列
queue CarInSouth;
queue CarInEast;
queue CarInNorth;
queue CarInWest;

//该方向是否有来车
bool SouthCome;
bool EastCome;
bool NorthCome;
bool WestCome;
//是否发生死锁，用于判断小车的下一步流程。
bool is_deadlock = false;

//四个方向来车运行流程
void *car_from_south(void *arg) {
    bool deadlock_flag = false;
    //等待被唤醒，首先是从队列唤醒，然后是可以进入第一个路段。
    pthread_mutex_lock(&WaitSouth);
    pthread_cond_wait(&FirstInSouth, &WaitSouth);
    pthread_mutex_unlock(&WaitSouth);

    //标识该方向有来车
    SouthCome = true;
    //给a上锁
    pthread_mutex_lock(&mutex_a);
    printf("car %d from South arrives at crossing\n", SouthId);
    //资源数量减1
    empty--;
    //表示自己的方向
    dir = south;
    //初始化为没有进行过等待
    bool flag = false;
    //如果资源用尽，发生死锁
    if (empty == 0) {
        //发生死锁，发信号给死锁线程。
        pthread_cond_signal(&cond_deadlock);
        //等待死锁被解除
        pthread_cond_wait(&cond_unlock, &mutex_a);
        //模拟停留时间
        usleep(2000);
        //锁b
        pthread_mutex_lock(&mutex_b);
        //解锁a
        pthread_mutex_unlock(&mutex_a);
        printf("car %d from South leaves at crossing\n", SouthId);
        //标识该方向没有来车
        SouthCome = false;
        //资源数加一
        empty++;
        //模拟停留在b
        usleep(2000);
        //解锁b
        pthread_mutex_unlock(&mutex_b);
        //死锁接触
        is_deadlock = false;
        //死锁接触后，唤醒所有方向队列中的车。
        if (CarInSouth.count > 0) {
            SouthId = CarInSouth.pop();
            pthread_cond_signal(&FirstInSouth);
        }
        if (CarInNorth.count > 0) {
            NorthId = CarInNorth.pop();
            pthread_cond_signal(&FirstInNorth);
        }
        if (CarInWest.count > 0) {
            WestId = CarInWest.pop();
            pthread_cond_signal(&FirstInWest);
        }
        if (CarInEast.count > 0) {
            EastId = CarInEast.pop();
            pthread_cond_signal(&FirstInEast);
        }
    }
        //如果右边有来车，需要让行等待b路口空出。
    else if (EastCome) {
        // 标识让行过。唤醒自己队列的车。
        flag = true;
        //等待右方的车将自己唤醒。
        pthread_cond_wait(&cond_s, &block_s);
        //如果在等待中发生了死锁，则需要唤醒自己左边的车。
        if (is_deadlock) {
            //模拟停留
            usleep(2000);
            //进入b，加锁
            pthread_mutex_lock(&mutex_b);
            //解锁a
            pthread_mutex_unlock(&mutex_a);
            printf("car %d from South leaves at crossing\n", SouthId);
            //如果发生死锁的方向是西，即其左手边，则解锁完毕，
            if (dir == west)
                pthread_cond_signal(&cond_unlock);
            else
                //否则，唤醒其左手边的车前进。
                pthread_cond_signal(&cond_w);
            //标识该方向没有来车。
            SouthCome = false;
            //资源数加1
            empty++;
            //模拟停留
            usleep(2000);
            //解锁b
            pthread_mutex_unlock(&mutex_b);
            //离开
            return NULL;
        }
    }
    //模拟停留
    usleep(2000);

    //没有上述情况，直接进入b
    pthread_mutex_lock(&mutex_b);

    //如果上一次有让行，因为右手边的车唤醒了自己，则需要帮助右手边的车唤醒右手边的车。
    if (CarInEast.count > 0 && flag) {
        EastId = CarInEast.pop();
        pthread_cond_signal(&FirstInEast);
    }

    //解锁a
    pthread_mutex_unlock(&mutex_a);
    //该方向没有来车
    SouthCome = false;
    //资源数量加1
    empty++;
    printf("car %d from South leaves at crossing\n", SouthId);

    //如果左手边有来车，则优先唤醒左手边来车，因为它等待过
    if (WestCome)
        pthread_cond_signal(&cond_w);
        //否则，唤醒自己方向来车
    else if (CarInSouth.count > 0) {
        SouthId = CarInSouth.pop();
        pthread_cond_signal(&FirstInSouth);
    }
    usleep(2000);
    //解锁资源b
    pthread_mutex_unlock(&mutex_b);
    //退出
    return NULL;
}

void *car_from_east(void *arg) {
    //等待被唤醒
    pthread_mutex_lock(&WaitEast);
    pthread_cond_wait(&FirstInEast, &WaitEast);
    pthread_mutex_unlock(&WaitEast);

    //表示该方向有来车
    EastCome = true;
    //进入b，锁住b资源
    pthread_mutex_lock(&mutex_b);
    printf("car %d from East arrives at crossing\n", EastId);
    //资源数减1
    empty--;
    //标识该方向为east
    dir = east;
    //初始化为没有等待过
    bool flag = false;
    //如果进入后资源数为0，则发生死锁
    if (empty == 0) {
        //给死锁线程发一个信号
        pthread_cond_signal(&cond_deadlock);
        //等待解锁信号
        pthread_cond_wait(&cond_unlock, &mutex_b);
        //模拟停留
        usleep(2000);
        //被唤醒后，进入c，锁住，并解锁b
        pthread_mutex_lock(&mutex_c);
        pthread_mutex_unlock(&mutex_b);
        printf("car %d from East leaves at crossing\n", EastId);
        //离开路口，标识该方向没有来车。
        EastCome = false;
        //资源数加1。
        empty++;
        //模拟停留
        usleep(2000);
        //离开c，解锁。
        pthread_mutex_unlock(&mutex_c);
        //死锁解开，设为false
        is_deadlock = false;
        //唤醒所有方向队列里等待的车
        if (CarInSouth.count > 0) {
            SouthId = CarInSouth.pop();
            pthread_cond_signal(&FirstInSouth);
        }
        if (CarInNorth.count > 0) {
            NorthId = CarInNorth.pop();
            pthread_cond_signal(&FirstInNorth);
        }
        if (CarInWest.count > 0) {
            WestId = CarInWest.pop();
            pthread_cond_signal(&FirstInWest);
        }
        if (CarInEast.count > 0) {
            EastId = CarInEast.pop();
            pthread_cond_signal(&FirstInEast);
        }
    }
    // 如果它的右手边有车
    else if (NorthCome) {
        //则需要等待，将等待设为true。
        flag = true;
        //等待右手边的车唤醒
        pthread_cond_wait(&cond_e, &block_e);
        //如果是从死锁中恢复过来
        if (is_deadlock) {
            //模拟停留
            usleep(2000);
            //经如c，锁住
            pthread_mutex_lock(&mutex_c);
            //解锁资源b
            pthread_mutex_unlock(&mutex_b);
            printf("car %d from East leaves at crossing\n", EastId);
            //如果是该方向车辆的左手边车造成死锁，则死锁已经解除
            if (dir == south)
                pthread_cond_signal(&cond_unlock);
                //否则，需要唤醒该方向左手边的车。
            else
                pthread_cond_signal(&cond_s);
            //完成，标识该方向没有来车
            EastCome = false;
            //模拟停留
            usleep(2000);
            //资源数加1
            empty++;
            //解锁c
            pthread_mutex_unlock(&mutex_c);
            return NULL;
        }
    }
    //模拟停留
    usleep(2000);
    //进入c，锁住
    pthread_mutex_lock(&mutex_c);

    //如果等待过右手边车辆，则需要帮助其唤醒下一辆车。
    if (!NorthCome && CarInNorth.count > 0 && flag) {
        NorthId = CarInNorth.pop();
        pthread_cond_signal(&FirstInNorth);
    }
    //资源数加1，解锁b
    empty++;
    pthread_mutex_unlock(&mutex_b);
    //标识该方向没有来车
    SouthCome = false;

    printf("car %d from East leaves at crossing\n", EastId);

    //如果左手边有来车等待，唤醒左手边车辆
    if (EastCome)
        pthread_cond_signal(&cond_s);
    //否侧唤醒自己方向的下一辆车。
    else if (CarInEast.count > 0)
    {
        EastId = CarInEast.pop();
        pthread_cond_signal(&FirstInEast);
    }
    //模拟停留
    usleep(2000);
    //解锁c，离开
    pthread_mutex_unlock(&mutex_c);
    return NULL;
}

void *car_from_north(void *arg) {
    //等待被唤醒
    pthread_mutex_lock(&WaitNorth);
    pthread_cond_wait(&FirstInNorth, &WaitNorth);
    pthread_mutex_unlock(&WaitNorth);
    //标识该方向有来车
    NorthCome = true;
    //锁住资源c
    pthread_mutex_lock(&mutex_c);
    printf("car %d from North arrives at crossing\n", NorthId);
    //资源数减1
    empty--;
    //标识方向
    dir = north;
    //是否等待右手边，初始化为否
    bool flag = false;
    //如果现在资源数为0，则发生死锁
    if (empty == 0) {
        //给死锁线程信号，开始解锁
        pthread_cond_signal(&cond_deadlock);
        //等待死锁线程发信号
        pthread_cond_wait(&cond_unlock, &mutex_c);

        //模拟停留
        usleep(2000);
        //经如d，锁住，解锁资源c
        pthread_mutex_lock(&mutex_d);
        pthread_mutex_unlock(&mutex_c);
        printf("car %d from West leaves at crossing\n", NorthId);
        //结束没有来车等待资源c
        NorthCome = false;
        //资源数加1
        empty++;
        //模拟停留
        usleep(2000);
        //解锁资源d
        pthread_mutex_unlock(&mutex_d);
        //死锁解除
        is_deadlock = false;
        //唤醒四个方向队列中的第一辆车
        if (CarInSouth.count > 0) {
            SouthId = CarInSouth.pop();
            pthread_cond_signal(&FirstInSouth);
        }
        if (CarInNorth.count > 0) {
            NorthId = CarInNorth.pop();
            pthread_cond_signal(&FirstInNorth);
        }
        if (CarInWest.count > 0) {
            WestId = CarInWest.pop();
            pthread_cond_signal(&FirstInWest);
        }
        if (CarInEast.count > 0) {
            EastId = CarInEast.pop();
            pthread_cond_signal(&FirstInEast);
        }
    }
    //如果他的右方有来车
    else if (WestCome) {
        // 等待过的标志设为true
        flag = true;
        // 等待右手边的车先走
        pthread_cond_wait(&cond_n, &block_n);
        //如果是从死锁中恢复过来，则需要唤醒左手边的车
        if (is_deadlock) {
            usleep(2000);
            pthread_mutex_lock(&mutex_d);
            pthread_mutex_unlock(&mutex_c);
            printf("car %d from North leaves at crossing\n", NorthId);
            if (dir == east)
                pthread_cond_signal(&cond_unlock);
            else
                pthread_cond_signal(&cond_e);
            NorthCome = false;
            //资源数加1.
            empty++;
            //模拟停留
            usleep(2000);
            //解锁资源d
            pthread_mutex_unlock(&mutex_d);
            return NULL;
        }
    }
    //模拟停留
    usleep(2000);
    //进入d，锁住
    pthread_mutex_lock(&mutex_d);
    //如果等待过右手边，则需要帮助右边的车辆唤醒下一辆
    if (CarInWest.count > 0 && flag) {
        WestId = CarInWest.pop();
        pthread_cond_signal(&FirstInWest);
    }
    //资源加1
    empty++;
    //解锁资源c
    pthread_mutex_unlock(&mutex_c);
    //结束，该方向标识为没有来车
    NorthCome = false;
    printf("car %d from North leaves at crossing\n", NorthId);
    //如果左边有来车等待，则唤醒左边的车，否则唤醒自己方向的下一辆车。
    if (EastCome)
        pthread_cond_signal(&cond_e);
    else if (CarInNorth.count > 0) {
        NorthId = CarInNorth.pop();
        pthread_cond_signal(&FirstInNorth);
    }
    //模拟停留
    usleep(2000);
    //解锁资源d
    pthread_mutex_unlock(&mutex_d);
    return NULL;
}

void *car_from_west(void *arg) {
    //等待唤醒
    pthread_mutex_lock(&WaitWest);
    pthread_cond_wait(&FirstInWest, &WaitWest);
    pthread_mutex_unlock(&WaitWest);

    //等待下一个资源，来车标识为true，锁住资源d
    WestCome = true;
    pthread_mutex_lock(&mutex_d);
    printf("car %d from West arrives at crossing\n", WestId);

    //资源数减1
    empty--;
    //标识该车方向
    dir = west;
    //是否等待过右车初始化为false
    bool flag = false;
    //如果empty为0，则发生死锁
    if (empty == 0) {
        //给死锁检测线程发送一个信号
        pthread_cond_signal(&cond_deadlock);
        //等待解锁
        pthread_cond_wait(&cond_unlock, &mutex_d);
        //模拟停留
        usleep(2000);
        //锁住资源a
        pthread_mutex_lock(&mutex_a);
        //解锁资源d
        pthread_mutex_unlock(&mutex_d);
        printf("car %d from West leaves at crossing\n", WestId);
        //结束，标识该方向没有来车
        WestCome = false;
        //资源数加1
        empty++;
        //模拟等待
        usleep(2000);
        //解锁资源a
        pthread_mutex_unlock(&mutex_a);
        //标识死锁解除
        is_deadlock = false;
        //唤醒所有方向等待队列的下一辆车
        if (CarInSouth.count > 0) {
            SouthId = CarInSouth.pop();
            pthread_cond_signal(&FirstInSouth);
        }
        if (CarInNorth.count > 0) {
            NorthId = CarInNorth.pop();
            pthread_cond_signal(&FirstInNorth);
        }
        if (CarInWest.count > 0) {
            WestId = CarInWest.pop();
            pthread_cond_signal(&FirstInWest);
        }
        if (CarInEast.count > 0) {
            EastId = CarInEast.pop();
            pthread_cond_signal(&FirstInEast);
        }
    }
        //如果右手边由来车，则让行。
    else if (SouthCome) {
        //标识等待过
        flag = true;
        //等待右手边车发送信号
        pthread_cond_wait(&cond_w, &block_w);
        //如果是从死锁中恢复过来，则需要唤醒左边的车。
        if (is_deadlock) {
            usleep(2000);
            pthread_mutex_lock(&mutex_a);
            pthread_mutex_unlock(&mutex_d);
            printf("car %d from West leaves at crossing\n", WestId);
            if (dir == north)
                pthread_cond_signal(&cond_unlock);
            else 
                pthread_cond_signal(&cond_n);
            //结束，标识该方向没有来车
            WestCome = false;
            //资源数量加1
            empty++;
            //模拟停留
            usleep(2000);
            //解锁资源a
            pthread_mutex_unlock(&mutex_a);
            return NULL;
        }
    }
    //模拟停留
    usleep(2000);
    //进入a，上锁
    pthread_mutex_lock(&mutex_a);

    //如果等待过右手边的车，则需要帮助其唤醒下一辆车。
    if (!NorthCome && CarInNorth.count > 0 && flag) {
        NorthId = CarInNorth.pop();
        pthread_cond_signal(&FirstInNorth);
    }
    //资源数加1
    empty++;
    pthread_mutex_unlock(&mutex_d);
    //结束，标识该方向没有来车。
    WestCome = false;
    printf("car %d from West leaves at crossing\n", WestId);
    //如果左手边有车等待，唤醒左手边的车。否则唤醒自己方向的下一辆车
    if (NorthCome)
        pthread_cond_signal(&cond_n);
    else if (CarInWest.count > 0) {
        WestId = CarInWest.pop();
        pthread_cond_signal(&FirstInWest);
    }
    //模拟停留
    usleep(2000);
    //解锁资源a。
    pthread_mutex_unlock(&mutex_a);
    return NULL;
}

void *check_dead_lock(void *arg) {
    //等待初始化后，其它线程运行到这一步
    usleep(4000);
    //初始化没有死锁
    is_deadlock = false;
    //唤醒所有四个方向等待队列的第一辆车
    if (CarInSouth.count > 0) {
        SouthId = CarInSouth.pop();
        pthread_cond_signal(&FirstInSouth);
    }
    if (CarInNorth.count > 0) {
        NorthId = CarInNorth.pop();
        pthread_cond_signal(&FirstInNorth);
    }
    if (CarInWest.count > 0) {
        WestId = CarInWest.pop();
        pthread_cond_signal(&FirstInWest);
    }
    if (CarInEast.count > 0) {
        EastId = CarInEast.pop();
        pthread_cond_signal(&FirstInEast);
    }
    //始终检测
    while (1) {
        pthread_mutex_lock(&mutex_e);
        //等待车辆线程发送死锁信号
        pthread_cond_wait(&cond_deadlock, &mutex_e);
        //死锁发生，标识为true
        is_deadlock = true;
        printf("DEADLOCK: car jam detected, signalling");
        //让最后一个进入死锁车的左手边车先走。
        switch (dir) {
            case north: {printf(" East "); pthread_cond_signal(&cond_e); break; }
            case east: {printf(" South "); pthread_cond_signal(&cond_s); break; }
            case west: {printf(" North "); pthread_cond_signal(&cond_n); break; }
            case south: {printf(" West "); pthread_cond_signal(&cond_w); break; }
        }
        printf("to go\n");

        pthread_mutex_unlock(&mutex_e);
    }
}

int main(int argc, char** argv) {
    //用于存储车的id
    int num[MAX];
    //创建死锁线程
    pthread_t check;
    //初始化信号量和条件变量
    pthread_cond_init(&cond_unlock, NULL);
    pthread_cond_init(&cond_deadlock, NULL);
    pthread_cond_init(&cond_w, NULL);
    pthread_cond_init(&cond_s, NULL);
    pthread_cond_init(&cond_e, NULL);
    pthread_cond_init(&cond_n, NULL);
    pthread_cond_init(&FirstInEast, NULL);
    pthread_cond_init(&FirstInWest, NULL);
    pthread_cond_init(&FirstInSouth, NULL);
    pthread_cond_init(&FirstInNorth, NULL);
    pthread_mutex_init(&mutex_a, NULL);
    pthread_mutex_init(&mutex_b, NULL);
    pthread_mutex_init(&mutex_c, NULL);
    pthread_mutex_init(&mutex_d, NULL);
    pthread_mutex_init(&mutex_e, NULL);
    pthread_mutex_init(&WaitNorth, NULL);
    pthread_mutex_init(&WaitSouth, NULL);
    pthread_mutex_init(&WaitEast, NULL);
    pthread_mutex_init(&WaitWest, NULL);
    pthread_mutex_init(&block_n, NULL);
    pthread_mutex_init(&block_e, NULL);
    pthread_mutex_init(&block_w, NULL);
    pthread_mutex_init(&block_s, NULL);

    char s[MAX];
    printf("----------Start----------\n");
    scanf("%s", s);
    int len = strlen(s);
    //初始化资源数为4
    empty = 4;
    //开始创建线程
    for (int i = 0; i<len; i++) {
        num[i] = i + 1;
        switch (s[i]) {
            case 'w': {
                CarInWest.push(i+1);//将车的id存起来
                car[size++] = CarInWest.thread[CarInWest.front];//将线程保存到car中
                pthread_create(&CarInWest.thread[CarInWest.front], NULL, car_from_west, NULL);//创建线程
                break;
            }
            case 'e': {
                CarInEast.push(i+1);
                car[size++] = CarInEast.thread[CarInEast.front];
                pthread_create(&CarInEast.thread[CarInEast.rear], NULL, car_from_east, NULL);
                break;
            }
            case 's': {
                CarInSouth.push(i+1);
                car[size++] = CarInSouth.thread[CarInSouth.rear];
                pthread_create(&CarInSouth.thread[CarInSouth.rear], NULL, car_from_south, NULL);
                break;
            }
            case 'n': {
                CarInNorth.push(i+1);
                car[size++] = CarInNorth.thread[CarInNorth.rear];
                pthread_create(&CarInNorth.thread[CarInNorth.rear], NULL, car_from_north, NULL);
                break;
            }
        }
    }

    pthread_create(&check, NULL, check_dead_lock, NULL);//创建死锁检测线程
    pthread_join(check,NULL);//等待所有小车线程结束
    for (int i = 0; i<size; i++) {
        pthread_join(car[i], NULL);
    }
}
