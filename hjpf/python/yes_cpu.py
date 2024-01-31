import time

def main():
    count = 0
    while True:
        print("yes")
        count += 1
        if count < 10000:
            time.sleep(max(0, 2/count))
        

if __name__ == '__main__':
    main()