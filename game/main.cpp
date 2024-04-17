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
#include <atomic>
#include <algorithm> // 引入算法库，用于 find_if
#include "font.h"
#include <mutex>
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
int speed = 200000;//初始速度
bool paused = false;  // 初始状态为未暂停

vector<struct pos> snakevector; //保存整个蛇每一节蛇身坐标位置
std::vector<pos> food_positions;  // 存储多个食物位置
std::vector<pos> poison_positions;  // 存储毒药位置的向量

//游戏最开始有三节蛇身
struct pos pos1={0,0};  //最开始蛇尾位置
struct pos pos2={32,0}; //中间的蛇身
struct pos pos3={64,0}; //最开始蛇头位置
//全局变量，控制方向标志位
int flag1 = 0; // x方向的移动标志，-1 左，1 右
int flag2 = 0; // y方向的移动标志，-1 上，1 下
const int MAX_X = 630;
const int MIN_X = 0;
const int MAX_Y = 470; // 假设最大的Y坐标为470
const int MIN_Y = 0; // 假设最小的Y坐标为0
const int SNAKE_STEP = 32; // 假设每次移动的像素为10   优化加速减速功能
void generateFood(pos& food);
extern void destroyBitmap(bitmap* bm);
void generatePoison();
int food_eaten = 0; // 线程安全的食物计数器
//struct LcdDevice* lcd; // 如果LcdDevice也需要前向声明
font *f;
std::mutex mutex_food_eaten;


//初始化Lcd
struct LcdDevice *init_lcd(const char *device)
{    
	//申请空间
	struct LcdDevice* lcd = (struct LcdDevice*)malloc(sizeof(struct LcdDevice));
	if(lcd == NULL)
	{
		return NULL;
	} 

	//1打开设备
	lcd->fd = open(device, O_RDWR);
	if(lcd->fd < 0)
	{
		perror("open lcd fail");
		free(lcd);
		return NULL;
	}
	
	//映射
	lcd->mp = (unsigned int*)mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd->fd,0);

	return lcd;
}

struct LcdDevice* lcd ;
// 显示食物计数到LCD屏幕
void updateFoodCountDisplay(struct LcdDevice* lcd, font *f) {
    printf("----------3-----------\n");
    bitmap *bm = createBitmapWithInit(60, 25, 4, getColor(255, 255, 255, 255));
    printf("----------4-----------\n");
    char buf[100];
    printf("----------5-----------\n");
    sprintf(buf, "%d", food_eaten);  // 将数字转换为字符串
    printf("buf = %s\n", buf);
    printf("----------6-----------\n");
    // 检查字体和位图状态
    if (f == NULL) {
        printf("Error: Font is NULL\n");
    }
    if (bm == NULL) {
        printf("Error: Bitmap is NULL\n");
    }
    // 尝试输出字体
    try {
        fontPrint(f, bm, 0, 0, buf, getColor(0, 255, 0, 255), 0);
    } catch (const std::exception& e) {
        printf("Exception caught in fontPrint: %s\n", e.what());
    }
    printf("----------7-----------\n");
    show_font_to_lcd(lcd->mp, 720, 235, bm); // 假设分数显示区域在720, 235位置
    printf("----------8-----------\n");
    destroyBitmap(bm);
    printf("----------9-----------\n");
}

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
			 // 暂停/继续按钮的判断
            if (ts_x >= 655 && ts_x <= 795 && ts_y >= 60 && ts_y <= 91) {
                // 切换暂停或继续状态
                paused = !paused;
                continue; // 跳过其余的逻辑
            }

            // 加速按钮的判断
            if (ts_x >= 655 && ts_x <= 795 && ts_y >= 115 && ts_y <= 150) {
                // 加速操作
                speed = max(speed - 20000, 100000); // 限制速度不小于某个值
                continue;
            }

            // 减速按钮的判断
            if (ts_x >= 655 && ts_x <= 795 && ts_y >= 170 && ts_y <= 208) {
                // 减速操作
                speed = min(speed + 20000, 300000); // 限制速度不超过某个值
                continue;
            }
			// 音乐播放按钮的判断
            if (ts_x >= 740 && ts_x <= 800 && ts_y >= 280 && ts_y <= 330) {
				// 检查是否有madplay进程正在运行
				if (system("pidof madplay") == 0) {
					// 如果正在运行，则先杀掉现有的madplay进程
					system("killall madplay");
					printf("Stopped existing music playback.\n");
				} else {
					// 播放bgm
					system("madplay 1.mp3 &");  // 使用 & 使madplay在后台运行
					printf("Started playing music.\n");
				}
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


//生成食物
void generateFood() {
    const int food_count = 2;  // 假设我们想生成5个食物
    for (int i = 0; i < food_count; ++i) {
        pos new_food;
        bool valid;
        do {
            new_food.x = (rand() % (MAX_X / SNAKE_STEP)) * SNAKE_STEP;
            new_food.y = (rand() % (MAX_Y / SNAKE_STEP)) * SNAKE_STEP;
            valid = true;
            // 检查是否与蛇身或其他食物重合
            for (const auto& snake_part : snakevector) {
                if (snake_part.x == new_food.x && snake_part.y == new_food.y) {
                    valid = false;
                    break;
                }
            }
            for (const auto& food : food_positions) {
                if (food.x == new_food.x && food.y == new_food.y) {
                    valid = false;
                    break;
                }
            }
        } while (!valid);
        food_positions.push_back(new_food);
    }
}

//生成毒药
void generatePoison() {
    const int poison_count = 5;  // 假设我们想生成5个毒药
    for (int i = 0; i < poison_count; ++i) {
        pos new_poison;
        bool valid;
        do {
            new_poison.x = (rand() % (MAX_X / SNAKE_STEP)) * SNAKE_STEP;
            new_poison.y = (rand() % (MAX_Y / SNAKE_STEP)) * SNAKE_STEP;
            valid = true;
            // 检查是否与蛇身、食物或其他毒药重合
            for (const auto& snake_part : snakevector) {
                if (snake_part.x == new_poison.x && snake_part.y == new_poison.y) {
                    valid = false;
                    break;
                }
            }
            for (const auto& food : food_positions) {
                if (food.x == new_poison.x && food.y == new_poison.y) {
                    valid = false;
                    break;
                }
            }
            for (const auto& poison : poison_positions) {
                if (poison.x == new_poison.x && poison.y == new_poison.y) {
                    valid = false;
                    break;
                }
            }
        } while (!valid);
        poison_positions.push_back(new_poison); // 假设 poison_positions 是存储毒药位置的 vector
    }
}



void *snakemove(void *arg) {
    generateFood();  // 首次生成多个食物
    generatePoison();  // 首次生成毒药
    while (true) {
        if (paused) {
            usleep(100000);  // 当游戏暂停时，仍然需要睡眠以避免CPU过度使用
            continue;  // 跳过此次循环的其余部分
        }

        pos new_head = {snakevector.back().x + flag1 * SNAKE_STEP, snakevector.back().y + flag2 * SNAKE_STEP};

        // 边界检查
        if (new_head.x < MIN_X || new_head.x > MAX_X || new_head.y < MIN_Y || new_head.y > MAX_Y) {
            cout << "对不起，蛇身移动超范围，游戏结束" << endl;
            pthread_exit(NULL);
        }

        bool ate_food = false;
        bool hit_poison = false;
        // 检查是否吃到任何一个食物
        auto it = food_positions.begin();
        while (it != food_positions.end()) {
            if (new_head.x == it->x && new_head.y == it->y) {
                snakevector.push_back(new_head);  // 增长蛇身
                ++food_eaten;  // 增加食物计数
                it = food_positions.erase(it);  // 移除被吃的食物
                printf("----------1-----------\n");
                printf("food_eaten = %d\n", food_eaten);
                printf("----------2-----------\n");
                updateFoodCountDisplay(lcd, f);  // 更新显示
                printf("food_eaten2 = %d\n", food_eaten);
                printf("----------10-----------\n");
                ate_food = true;
                break;
            } else {
                ++it;
            }
        }

        // 检查是否触碰到毒药
        auto itp = poison_positions.begin();
        while (itp != poison_positions.end()) {
            if (new_head.x == itp->x && new_head.y == itp->y) {
                // 可以选择缩短蛇身，或游戏结束等
                cout << "对不起，触碰到毒药，游戏结束" << endl;
                pthread_exit(NULL);  // 示例：游戏直接结束
            } else {
                ++itp;
            }
        }

        if (ate_food) {
            generateFood();  // 生成新的食物
        }
        
        if (!ate_food && !hit_poison) {
            snakevector.push_back(new_head);
            const pos &temppos = snakevector.front();
            show_anybmp(32, 32, temppos.x, temppos.y, "./bmp/grass1.bmp");
            snakevector.erase(snakevector.begin());
        }

        // 更新蛇身
        for (const auto &curpos : snakevector) {
            show_anybmp(32, 32, curpos.x, curpos.y, "./bmp/red.bmp");
        }
        // 显示所有食物
        for (const auto &food : food_positions) {
            show_anybmp(32, 32, food.x, food.y, "./bmp/food.bmp");
        }
        // 显示所有毒药
        for (const auto &poison : poison_positions) {
            show_anybmp(32, 32, poison.x, poison.y, "./bmp/poison.bmp");
        }

        usleep(speed);  // 控制游戏速度，使用全局变量speed
    }
    return NULL;
}




int main()
{
	pthread_t tsid;//读取触摸屏坐标
	pthread_t snakeid;//蛇
	 
	
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
	
	//初始化Lcd
	//printf("------444------\n");
	lcd = init_lcd("/dev/fb0");
	//printf("------666------\n");		
	show_anybmp(800,480,0,0,"./bmp/ui1.bmp");
	//打开字体	
	char fontPath[] = "/usr/share/fonts/DroidSansFallback.ttf";
    f = fontLoad("/usr/share/fonts/DroidSansFallback.ttf");
	if (f == NULL) {
    fprintf(stderr, "Failed to load font\n");
    // 清理并退出
    return -1;
}

	//字体大小的设置
	fontSetSize(f,30);
	
	//创建线程读取触摸屏坐标
	pthread_create(&tsid,NULL,readts,NULL);
	bool run = true;
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
	


	 // 清理
    fontUnload(f);
	return 0;
}