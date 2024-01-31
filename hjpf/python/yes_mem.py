import time

def main():
    ls = "yes"
    while True:
        ls = ls + ls
        time.sleep(0.5)

if __name__ == '__main__':
    main()