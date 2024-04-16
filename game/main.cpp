#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <algorithm> // 引入算法库，用于 find_if
using namespace std;
struct pos
{
	int x; //一节蛇身左上角x坐标
	int y; //一节蛇身左上角y坐标
};

extern int show_anybmp(int w,int h,int x,int y,const char *bmpname);

void generateFood(); // 函数声明，必须在 generateFood 的实际定义前
int ts_x; //保存触摸屏x坐标
int ts_y; //保存触摸屏y坐标
int tsfd; //保存触摸屏的文件描述符
vector<struct pos> snakevector; //保存整个蛇每一节蛇身坐标位置
std::vector<pos> food_positions;  // 存储多个食物位置

//游戏最开始有三节蛇身
struct pos pos1={0,0};  //最开始蛇尾位置
struct pos pos2={20,0}; //中间的蛇身
struct pos pos3={40,0}; //最开始蛇头位置
//全局变量，控制方向标志位
int flag1 = 0; // x方向的移动标志，-1 左，1 右
int flag2 = 0; // y方向的移动标志，-1 上，1 下
const int MAX_X = 630;
const int MIN_X = 0;
const int MAX_Y = 470; // 假设最大的Y坐标为470
const int MIN_Y = 0; // 假设最小的Y坐标为0
const int SNAKE_STEP = 10; // 假设每次移动的像素为10   优化加速减速功能
void generateFood(pos& food);

void *readts(void *arg)
{
	//定义结构体存放坐标值
	struct input_event myevent;
	while(1)
	{
		//读取触摸屏坐标
		read(tsfd,&myevent,sizeof(myevent));
		//判断x,y打印出来
		if (myevent.type == EV_ABS) {
			if (myevent.code == ABS_X) {
				ts_x = (800 * myevent.value) / 1024;
			}
			if (myevent.code == ABS_Y) {
				ts_y = (480 * myevent.value) / 600;
			}
			// 判断控制方向
			if (ts_x >= 700 && ts_x <= 740) {
				if (ts_y >= 320 && ts_y <= 380) { // 向上
					flag1 = 0; flag2 = -1;
				} else if (ts_y >= 410 && ts_y <= 470) { // 向下
					flag1 = 0; flag2 = 1;
				}
			}
			if (ts_y >= 370 && ts_y <= 415) {
				if (ts_x >= 650 && ts_x <= 705) { // 向左
					flag1 = -1; flag2 = 0;
				} else if (ts_x >= 730 && ts_x <= 795) { // 向右
					flag1 = 1; flag2 = 0;
				}
			}
		}
     }
}



// 随机生成食物位置，确保食物不在蛇身上
void generateFood() {
    const int food_count = 5;  // 假设我们想生成5个食物
    food_positions.clear();
    for (int i = 0; i < food_count; ++i) {
        pos new_food;
        bool valid;
        do {
            new_food.x = rand() % (MAX_X / SNAKE_STEP) * SNAKE_STEP;
            new_food.y = rand() % (MAX_Y / SNAKE_STEP) * SNAKE_STEP;
            valid = true;

            // 检查新食物是否与蛇身重合
            for (const auto& snake_part : snakevector) {
                if (snake_part.x == new_food.x && snake_part.y == new_food.y) {
                    valid = false;
                    break;
                }
            }

            // 只有当新食物未与蛇身重合时，才检查是否与其他食物重合
            if (valid) {
                for (const auto& existing_food : food_positions) {
                    if (existing_food.x == new_food.x && existing_food.y == new_food.y) {
                        valid = false;
                        break;
                    }
                }
            }
        } while (!valid);
        food_positions.push_back(new_food);  // 添加新食物位置到列表
    }
}


void *snakemove(void *arg) {
    generateFood();  // 首次生成多个食物

    while (true) {
        pos new_head = {snakevector.back().x + flag1 * SNAKE_STEP, snakevector.back().y + flag2 * SNAKE_STEP};

        // 边界检查
        if (new_head.x < MIN_X || new_head.x > MAX_X || new_head.y < MIN_Y || new_head.y > MAX_Y) {
            cout << "对不起，蛇身移动超范围，游戏结束" << endl;
            pthread_exit(NULL);
        }

        bool ate_food = false;
        // 检查是否吃到任何一个食物
        auto it = food_positions.begin();
        while (it != food_positions.end()) {
            if (new_head.x == it->x && new_head.y == it->y) {
                snakevector.push_back(new_head);  // 增长蛇身
                it = food_positions.erase(it);    // 移除被吃的食物
                generateFood();  // 生成新的食物
                ate_food = true;
                break;
            } else {
                ++it;
            }
        }

        if (!ate_food) {
            snakevector.push_back(new_head);
            const pos &temppos = snakevector.front();
            show_anybmp(20, 20, temppos.x, temppos.y, "./bmp/white1.bmp");
            snakevector.erase(snakevector.begin());
        }

        // 更新蛇身
        for (const auto &curpos : snakevector) {
            show_anybmp(20, 20, curpos.x, curpos.y, "./bmp/red1.bmp");
        }
        // 显示所有食物
        for (const auto &food : food_positions) {
            show_anybmp(20, 20, food.x, food.y, "./bmp/food.bmp");
        }

        usleep(100000);  // 控制游戏速度0.1s
    }
    return NULL;
}




int main()
{
	pthread_t tsid;
	pthread_t snakeid;
	
	//保存三节蛇身(0,0)(10,0)(20,0)
	snakevector.push_back(pos1);
	snakevector.push_back(pos2);
	snakevector.push_back(pos3);
	
	//打开触摸屏的驱动
	tsfd=open("/dev/input/event0",O_RDWR);
	if(tsfd==-1)
	{
		printf("打开触摸屏失败!\n");
		return -1;
	}
	
	//显示游戏背景
	show_anybmp(800,480,0,0,"./bmp/ui.bmp");
	
	//创建线程读取触摸屏坐标
	pthread_create(&tsid,NULL,readts,NULL);
	
	//进入死循环判断触摸屏点击坐标位置
	while(1)
	{
		if(ts_x>655&&ts_x<795&&ts_y>8&&ts_y<41) //开始游戏
		{
			cout<<"游戏开始"<<endl;
			//在(0,0)位置绘制3节蛇身，并且默认自动往右开始移动(后面由用户来控制方向)
			pthread_create(&snakeid,NULL,snakemove,NULL);
			ts_x=0;
			ts_y=0;
		}	
	}
	return 0;
}