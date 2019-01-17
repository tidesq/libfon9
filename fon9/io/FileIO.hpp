/// \file fon9/io/FileIO.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FileIO_hpp__
#define __fon9_io_FileIO_hpp__
#include "fon9/io/Device.hpp"
#include "fon9/io/RecvBuffer.hpp"
#include "fon9/File.hpp"
#include "fon9/Timer.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 使用 File 模擬 io 通訊.
/// - 設定參數:
///   - InFN = input file name
///   - InMode = input file open mode, default=CreatePath
///   - InStart = 開檔後從哪開始讀取(file offset), default=0.
///   - InSpeed = InSize*InInterval
///      - InSize = 每次讀取的大小, default=0. 若為0, 則視 BufferNode 的容量而定.
///      - InInterval = 每次讀取間隔秒數(使用 TimeInterval 格式), default=1ms
///      - 預設為 "0*1ms"
///   - InEOF = 輸入檔讀到EOF時的處理方式
///      - InEOF=Dontcare 預設值, 不理會EOF, 持續使用 InInterval 的間隔嘗試讀取.
///      - InEOF=Broken   變成 State::LinkBroken 狀態, 不再讀取.
///                       但 Manager 或 Session 可能會延遲一段時間後呼叫 AsyncOpen();
///      - InEOF=Close
///      - InEOF=Dispose
///      - InEOF=n        n>0: delay seconds 在 n 秒後再次嘗試讀取.
///   - OutFN = output filename (僅支援 Append 模式)
///   - OutMode = output file open mode, default=CreatePath
///   - 若沒設定 OutFN, 則在 Send() 時, 使用 ConsumeErr(SysErr::not_supported);
/// - file mode: 參考 StrToFileMode()
/// - file name: 允許使用 TimedFileName 格式, 但 Open 之後就不再變更檔名.
///              所以如果要依時間重開, 則應使用 IoManager 的排程機制.
/// - DeviceId:
///   - Input mode:  "|I=InFN"
///   - Output mode: "|O=OutFN"
///   - I+O mode:    "|I=InFN|O=OutFN"
/// - DeviceInfo:
///   - LinkReady: ("I" or "O" or "I+O") + "|InCur=|InFileTime=|InFileSize=|InSpeed=n*ti"
class fon9_API FileIO : public Device {
   fon9_NON_COPY_NON_MOVE(FileIO);
   using base = Device;
public:
   FileIO(SessionSP ses, ManagerSP mgr, const DeviceOptions* optsDefault = nullptr)
      : base(std::move(ses), std::move(mgr), Style::Simulation, optsDefault) {
   }

   bool IsSendBufferEmpty() const override;
   SendResult SendASAP(const void* src, size_t size) override;
   SendResult SendASAP(BufferList&& src) override;
   SendResult SendBuffered(const void* src, size_t size) override;
   SendResult SendBuffered(BufferList&& src) override;

protected:
   void OpImpl_Open(std::string cfgstr) override;
   void OpImpl_Reopen() override;
   void OpImpl_Close(std::string cause) override;
   void OpImpl_AppendDeviceInfo(std::string& info) override;
   void OpImpl_StartRecv(RecvBufferSize preallocSize) override;

private:
   enum class WhenEOF {
      Dontcare = 0,
      Broken = -1,
      Close = -2,
      Dispose = -3,
      // > 0: long delay seconds.
   };
   struct InProps {
      File::SizeType StartPos_{0};
      File::SizeType ReadSize_{0}; // 每次期望的讀入資料量.
      TimeInterval   Interval_;
      WhenEOF        WhenEOF_{WhenEOF::Dontcare};
      InProps();
      bool Parse(StrView tag, StrView value);
      void AppendInfo(std::string& msg);
   };
   InProps  InProps_;

   struct FileCfg {
      FileMode    Mode_;
      std::string FileName_;// TimedFileName?
      File::Result Open(File& fd, TimeStamp tm) const;
   };
   FileCfg  InFileCfg_;
   FileCfg  OutFileCfg_;

   enum class InFileSt {
      Readable,
      AtEOF,
      ReadError,
   };
   using FileIOSP = intrusive_ptr<FileIO>;
   struct Impl : public TimerEntry {
      fon9_NON_COPY_NON_MOVE(Impl);
      const FileIOSP Owner_;
      File           InFile_;
      RecvBuffer     InBuffer_;
      InFileSt       InSt_{InFileSt::Readable};
      File::PosType  InCurPos_{0};
      RecvBufferSize RecvSize_{RecvBufferSize::Default}; // Session 期望的接收緩衝區大小.
      File           OutFile_;
      BufferList     OutBuffer_; // 使用 StartSendChecker 保護.

      Impl(FileIO& owner)
         : TimerEntry{GetDefaultTimerThread()}
         , Owner_{&owner}
         , InCurPos_{owner.InProps_.StartPos_} {
      }
      void OnTimer(TimeStamp now) override;
      void CheckContinueRecv();
      void AsyncFlushOutBuffer(DeviceOpQueue::ALockerForInplace& alocker);
   };
   using ImplSP = intrusive_ptr<Impl>;
   ImplSP   ImplSP_;

   void OpImpl_StopRunning();
   void OpenImpl();
   bool OpenFile(StrView errMsgHead, const FileCfg& cfg, File& fd, TimeStamp tm);

   // - InPos=pos   設定讀取位置
   // - InSpeed=InSize*InInterval
   // - WhenEOF=Broken or Close or Dispose
   ConfigParser::Result OpImpl_SetProperty(StrView tag, StrView& value);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_FileIO_hpp__
