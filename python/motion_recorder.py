import asyncio
from bleak import BleakClient, BleakScanner
import struct
import csv
import os
import re
import matplotlib
import matplotlib.pyplot as plt
from datetime import datetime
import threading

# 🔧 Matplotlib の GUI を無効化（画像のみ描画）
matplotlib.use("Agg")

# BLE設定
DEVICE_NAME = "M5Stick_Motion"
SERVICE_UUID = "5fafc201-1fb5-459e-8fcc-c5c9c331914c"
CHARACTERISTIC_UUID = "ceb5483e-36e1-4688-b7f5-ea07361b26a9"


class MotionRecorder:
    def __init__(self, loop):
        self.data_buffer = []
        self.is_recording = False
        self.last_saved_file = None
        self.loop = loop  # メインのイベントループを保持

    def get_next_filename(self):
        """次に保存するファイル名を取得"""
        save_dir = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "motion_data"
        )
        os.makedirs(save_dir, exist_ok=True)

        existing_files = os.listdir(save_dir)
        pattern = r"crap_(\d{3})\.csv"
        existing_numbers = [
            int(re.match(pattern, f).group(1))
            for f in existing_files
            if re.match(pattern, f)
        ]

        next_number = max(existing_numbers, default=0) + 1
        return os.path.join(save_dir, f"crap_{next_number:03d}.csv")

    def start_recording(self):
        """記録開始"""
        self.is_recording = True
        self.data_buffer = []
        print("\n📂 記録を開始しました！")

    def stop_recording(self):
        """記録停止とデータ保存"""
        print(f"🛑 記録停止関数が呼ばれました (is_recording={self.is_recording})")
        if self.is_recording:
            self.is_recording = False
            self.save_motion_data()
            print("\n📂 記録を停止しました！")

            # 🔧 メインスレッドでプロットを実行
            self.loop.call_soon_threadsafe(self.plot_from_csv)

    def save_motion_data(self):
        """加速度データをCSVファイルに保存"""
        if self.data_buffer:
            filename = self.get_next_filename()
            with open(filename, "w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(["timestamp", "acc_x", "acc_y", "acc_z"])
                writer.writerows(self.data_buffer)
            self.last_saved_file = filename
            print(f"✅ データが保存されました: {filename}")

    def plot_from_csv(self):
        """保存したCSVからグラフを作成し保存"""
        if not self.last_saved_file:
            print("⚠ グラフを作成するデータがありません。")
            return

        acc_x, acc_y, acc_z = [], [], []
        with open(self.last_saved_file, "r") as f:
            reader = csv.reader(f)
            next(reader)
            for row in reader:
                acc_x.append(float(row[1]))
                acc_y.append(float(row[2]))
                acc_z.append(float(row[3]))

        plt.figure(figsize=(10, 6))
        plt.plot(acc_x, "r-", label="X軸")
        plt.plot(acc_y, "g-", label="Y軸")
        plt.plot(acc_z, "b-", label="Z軸")
        plt.title("加速度センサーデータ")
        plt.xlabel("サンプル番号")
        plt.ylabel("加速度")
        plt.legend()
        plt.grid()

        graph_dir = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "motion_graph"
        )
        os.makedirs(graph_dir, exist_ok=True)
        graph_filename = os.path.join(
            graph_dir, os.path.basename(self.last_saved_file).replace(".csv", ".png")
        )
        plt.savefig(graph_filename)
        print(f"📊 グラフを保存しました: {graph_filename}")
        plt.close()

    def notification_handler(self, sender, data):
        """BLEからのデータ受信処理"""
        try:
            acc_x, acc_y, acc_z = struct.unpack("fff", data)
            timestamp = datetime.now().strftime(
                "%Y-%m-%d %H:%M:%S.%f"
            )  # ミリ秒まで記録
            if self.is_recording:
                self.data_buffer.append([timestamp, acc_x, acc_y, acc_z])
        except Exception as e:
            print(f"❌ データ処理エラー: {e}")


async def connect_device(name_prefix, handler):
    """デバイス接続処理"""
    devices = await BleakScanner.discover()
    for device in devices:
        if device.name and name_prefix in device.name:
            print(f"🔗 接続中: {device.name} ({device.address})")
            client = BleakClient(device.address)
            try:
                await client.connect()
                await client.start_notify(CHARACTERISTIC_UUID, handler)
                print("✅ 接続成功！データ受信開始")
                return client
            except Exception as e:
                print(f"❌ 接続エラー: {e}")
                await client.disconnect()
    print("⚠ M5StickCが見つかりません")
    return None


def command_listener(recorder):
    """ユーザーコマンドを処理する別スレッド"""
    while True:
        cmd = input("\nコマンドを入力 (r: 記録開始, s: 記録終了, q: 終了): ")
        if cmd.lower() == "r":
            recorder.start_recording()
        elif cmd.lower() == "s":
            recorder.stop_recording()
        elif cmd.lower() == "q":
            if recorder.is_recording:
                recorder.stop_recording()
            print("\n🛑 プログラムを終了します")
            os._exit(0)  # 強制終了


async def main():
    loop = asyncio.get_running_loop()
    recorder = MotionRecorder(loop)
    client = await connect_device(DEVICE_NAME, recorder.notification_handler)
    if not client:
        print("🔄 5秒後に再試行...")
        await asyncio.sleep(5)
        return

    # 別スレッドでユーザーコマンドを処理
    threading.Thread(target=command_listener, args=(recorder,), daemon=True).start()

    try:
        while True:
            await asyncio.sleep(1)  # データ受信を続けるためのスリープ
    except KeyboardInterrupt:
        print("\n🛑 プログラムを終了します")
    finally:
        await client.disconnect()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n🛑 プログラムを終了します")
