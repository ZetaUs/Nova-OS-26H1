import pygame
import sys

# 初始化 Pygame
pygame.init()

# 设置一些常量
WIDTH, HEIGHT = 640, 480
LOGO_WIDTH, LOGO_HEIGHT = 80, 80
SCALE = 4  # 调节 Logo 的放大倍数

# 创建显示窗口
screen = pygame.display.set_mode((WIDTH, HEIGHT))

# 加载 Logo 数据
with open('logo_data.h', 'r') as f:
    lines = f.readlines()

logo_data = [0] * (LOGO_WIDTH * LOGO_HEIGHT)
i, j = 0, 0
for line in lines:
    line = line.strip()
    for val in line.split():
        if val.isdigit():
            logo_data[i * LOGO_WIDTH + j] = int(val)
            i += 1
            if i >= LOGO_HEIGHT:
                i = 0
                j += 1
                if j >= LOGO_WIDTH:
                    break

# 函数用于绘制 Logo
def draw_logo(x, y):
    for row in range(LOGO_HEIGHT):
        for col in range(LOGO_WIDTH):
            color = logo_data[row * LOGO_WIDTH + col]
            if color != 0:
                pygame.draw.rect(screen, (color, color, color), (x + col * SCALE, y + row * SCALE, SCALE, SCALE))  # 绘制一个像素

# 主循环
while True:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            sys.exit()

    # 绘制 Logo
    screen.fill((0, 0, 0))  # 清除屏幕
    draw_logo((WIDTH - LOGO_WIDTH * SCALE) // 2, (HEIGHT - LOGO_HEIGHT * SCALE) // 2)
    pygame.display.flip()

    # 限制帧率
    pygame.time.Clock().tick(60)
