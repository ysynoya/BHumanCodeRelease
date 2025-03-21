/**
 * @file LogExtractor.cpp
 *
 * Implementation of class LogExtractor
 *
 * @author Jan Fiedler
 */

#include "LogExtractor.h"
#include "ImageExport.h"
#include "LogPlayer.h"
#include "MathBase/RingBuffer.h"
#include "Platform/File.h"
#include "Representations/Infrastructure/AudioData.h"
#include "Representations/Infrastructure/CameraImage.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Infrastructure/JointAngles.h"
#include "Representations/Infrastructure/JPEGImage.h"
#include "Representations/Infrastructure/SensorData/RawInertialSensorData.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/Perception/ImagePreprocessing/ImageCoordinateSystem.h"
#include "Representations/Sensing/FallDownState.h"
#include "Framework/LoggingTools.h"
#include <filesystem>

/**
 * Example syntax:
 * DECLARE_REPRESENTATIONS_AND_MAP(
 * {,
 *   BallModel,
 *   BallPercept,
 *   RobotPose,
 * });
 */
#define DECLARE_REPRESENTATIONS_AND_MAP(brace, ...) \
  _DECLARE_REPRESENTATIONS_AND_MAP_I(_STREAM_TUPLE_SIZE(__VA_ARGS__), __VA_ARGS__)

#define _DECLARE_REPRESENTATIONS_AND_MAP_I(n, ...) \
  _DECLARE_REPRESENTATIONS_AND_MAP_II(n, (_DECLARE_REPRESENTATIONS_AND_MAP_DECLARE, __VA_ARGS__), (_DECLARE_REPRESENTATIONS_AND_MAP_LIST, __VA_ARGS__))
#define _DECLARE_REPRESENTATIONS_AND_MAP_II(n, declare, list) \
  _STREAM_ATTR_##n declare std::map<const MessageID, Streamable*> representations = { _STREAM_ATTR_##n list };

#define _DECLARE_REPRESENTATIONS_AND_MAP_DECLARE(type) type the##type;
// The extra comma for the last representation seems to be no problem.
#define _DECLARE_REPRESENTATIONS_AND_MAP_LIST(type) { id##type, &the##type },

LogExtractor::LogExtractor(LogPlayer& logPlayer) : logPlayer(logPlayer) {}

bool LogExtractor::saveAudioFile(const std::string& fileName)
{
  OutBinaryFile stream(fileName);
  if(!stream.exists())
    return false;

  int frames = 0;
  AudioData audioData;
  for(MessageQueue::Message message : logPlayer)
    if(logPlayer.id(message) == idAudioData)
    {
      message.bin() >> audioData;
      frames += unsigned(audioData.samples.size()) / audioData.channels;
    }

  struct WAVHeader
  {
    char chunkId[4];
    int chunkSize;
    char format[4];
    char subchunk1Id[4];
    int subchunk1Size;
    short audioFormat;
    short numChannels;
    int sampleRate;
    int byteRate;
    short blockAlign;
    short bitsPerSample;
    char subchunk2Id[4];
    int subchunk2Size;
  };

  int length = sizeof(WAVHeader) + sizeof(AudioData::Sample) * frames * audioData.channels;
  WAVHeader* header = reinterpret_cast<WAVHeader*>(new char[length]);
  *reinterpret_cast<unsigned*>(header->chunkId) = *reinterpret_cast<const unsigned*>("RIFF");
  header->chunkSize = length - 8;
  *reinterpret_cast<unsigned*>(header->format) = *reinterpret_cast<const unsigned*>("WAVE");

  *reinterpret_cast<unsigned*>(header->subchunk1Id) = *reinterpret_cast<const unsigned*>("fmt ");
  header->subchunk1Size = 16;
  static_assert(std::is_same<AudioData::Sample, short>::value
                || std::is_same<AudioData::Sample, float>::value, "Wrong audio sample type");
  header->audioFormat = std::is_same<AudioData::Sample, short>::value ? 1 : 3;
  header->numChannels = static_cast<short>(audioData.channels);
  header->sampleRate = audioData.sampleRate;
  header->byteRate = audioData.sampleRate * audioData.channels * sizeof(AudioData::Sample);
  header->blockAlign = short(audioData.channels * sizeof(AudioData::Sample));
  header->bitsPerSample = short(sizeof(AudioData::Sample) * 8);

  *reinterpret_cast<unsigned*>(header->subchunk2Id) = *reinterpret_cast<const unsigned*>("data");
  header->subchunk2Size = frames * audioData.channels * sizeof(AudioData::Sample);

  char* p = reinterpret_cast<char*>(header + 1);
  for(MessageQueue::Message message : logPlayer)
    if(logPlayer.id(message) == idAudioData)
    {
      message.bin() >> audioData;
      memcpy(p, audioData.samples.data(), audioData.samples.size() * sizeof(AudioData::Sample));
      p += audioData.samples.size() * sizeof(AudioData::Sample);
    }

  stream.write(header, length);
  delete[] header;

  return true;
}

bool LogExtractor::saveImages(const std::string& path, const bool raw, const bool onlyPlaying, const int takeEachNthFrame = 1)
{
  class CRCLut : public std::array<unsigned int, 256>
  {
  public:
    CRCLut() : std::array<unsigned int, 256>()
    {
      for(unsigned int n = 0; n < 256; n++)
      {
        unsigned int c = n;
        for(unsigned int k = 0; k < 8; k++)
        {
          if(c & 1)
            c = 0xedb88320L ^ (c >> 1);
          else
            c = c >> 1;
        }
        (*this)[n] = c;
      }
    }
  };

  class CRC
  {
  private:
    unsigned int crc;
  public:
    CRC() : crc(0xffffffff) {}
    CRC(const unsigned int initialCrc) : crc(initialCrc) {}

    CRC update(const CRCLut& lut, const void* data, const size_t size) const
    {
      const unsigned char* dataPtr = reinterpret_cast<const unsigned char*>(data);
      unsigned int crc = this->crc;

      for(size_t i = 0; i < size; i++)
        crc = lut[(crc ^ dataPtr[i]) & 0xff] ^ (crc >> 8);

      return CRC(crc);
    }

    unsigned int finish() const
    {
      return crc ^ 0xffffffff;
    }
  };

  DECLARE_REPRESENTATIONS_AND_MAP(
  {,
    CameraInfo,
    CameraMatrix,
    FrameInfo,
    ImageCoordinateSystem,

    // To find valid images
    FallDownState,
    GameState,
    CameraImage,
    JPEGImage,
  });

  std::string folderPath = File::isAbsolute(path.c_str()) ? path : std::string(File::getBHDir()) + "/Config/" + path;
  std::filesystem::create_directories(folderPath);

  const CRCLut crcLut;

  int skippedImageCount = 0;

  // Use DECLARE_REPRESENTATIONS_AND_MAP as soon as the hack is no longer needed
  return goThroughLog(
           representations,
           [&](const std::string&)
  {
    if(onlyPlaying &&
       (!theGameState.isPlaying() // isStateValid
        || theGameState.isPenalized() // isNotPenalized
        || (theFallDownState.state != FallDownState::upright
            && theFallDownState.state != FallDownState::staggering)/*isStanding*/))
      return true;

    if(theJPEGImage.timestamp)
    {
      theJPEGImage.toCameraImage(theCameraImage); // Assume that CameraImage and JPEGImage are not logged at the same time.
      theJPEGImage.timestamp = 0;
    }

    CameraImage* imageToExport = nullptr;

    if(theCameraImage.timestamp)
      imageToExport = &theCameraImage;

    if(imageToExport)
    {
      // Frame skipping: only count frames if they are from the upper camera so
      // that always a pair of lower and upper frames is saved
      if(theCameraInfo.camera == CameraInfo::upper && ++skippedImageCount == takeEachNthFrame)
        skippedImageCount = 0;
      if(skippedImageCount != 0)
        return true;

      // Open PNG file
      const std::string filename = ImageExport::expandImageFileName(folderPath + TypeRegistry::getEnumName(theCameraInfo.camera), imageToExport->timestamp);
      QFile qfile(filename.c_str());
      qfile.open(QIODevice::WriteOnly);

      // Write image
      ImageExport::exportImage(*imageToExport, qfile, raw ? ImageExport::raw : ImageExport::rgb);
      imageToExport->timestamp = 0;

      // Remove IEND chunk
      qfile.resize(qfile.size() - 12);

      // Write metadata
      OutBinaryMemory metaData;
      metaData << theCameraInfo;
      metaData << theCameraMatrix;
      metaData << theImageCoordinateSystem;
      const unsigned int size = static_cast<unsigned int>(metaData.size());
      for(size_t i = 0; i < 4; i++)
        qfile.putChar(reinterpret_cast<const char*>(&size)[3 - i]);
      qfile.write("bhMn");
      qfile.write(metaData.data(), metaData.size());
      const unsigned int crc = CRC().update(crcLut, "bhMn", 4).update(crcLut, metaData.data(), metaData.size()).finish();
      for(size_t i = 0; i < 4; i++)
        qfile.putChar(reinterpret_cast<const char*>(&crc)[3 - i]);

      // Write IEND chunk
      const std::array<char, 12> endChunk{ 0, 0, 0, 0, 'I', 'E', 'N', 'D', char(0xae), char(0x42), char(0x60), char(0x82) };
      qfile.write(endChunk.data(), endChunk.size());
      qfile.close();
    }

    return true;
  });
}

bool LogExtractor::analyzeRobotStatus()
{
  DECLARE_REPRESENTATIONS_AND_MAP(
  {,
    JointAngles,
    FrameInfo,
    RawInertialSensorData,
  });

  RingBuffer<JointAngles, 5> angleList;
  Vector3a lastGyro;
  int frameCounter = -1;
  int disconnectCounter = -1;

  bool finished = goThroughLog(
                    representations,
                    [&angleList, &theJointAngles, &lastGyro, &disconnectCounter, &theRawInertialSensorData, &frameCounter, &theFrameInfo](const std::string&)
  {
    frameCounter += 1; //for some reason the frameCounter can get desynced with the real frame number. Also is there a better way to get the log frame?
    //only continue, if the JointAngles have new data

    disconnectCounter += 1;
    bool canContinue = false;
    //only continue, if the JointAngles have new data
    if(theRawInertialSensorData.gyro != lastGyro)
    {
      disconnectCounter = 0;
      lastGyro = theRawInertialSensorData.gyro;
    }
    if(disconnectCounter > 5)
      OUTPUT_TEXT("Gyros not updating at LogFrame: " << frameCounter);

    for(size_t i = 0; i < Joints::numOfJoints; i++)
      if(theJointAngles.angles[i] != angleList[0].angles[i])
        canContinue = true;
    if(!canContinue)
      return true;
    angleList.push_front(theJointAngles);
    //wait until the RingBuffer is filled with the last 5 JointAngles
    if(frameCounter >= 5)
    {
      std::vector<JointAngles> difs;
      JointAngles jointAnglePre = angleList[0];
      JointAngles jointAnglePost;
      for(size_t i = 1; i < angleList.capacity(); i++)
      {
        JointAngles angles;
        jointAnglePost = angleList[i];

        //calc the difference of the 2 JointAngles
        for(size_t j = 0; j < Joints::numOfJoints; j++)
        {
          Angle a = jointAnglePre.angles[j];
          Angle b = jointAnglePost.angles[j];
          angles.angles[j] = a - b;
        }
        difs.push_back(angles);
        jointAnglePre = jointAnglePost;
      }
      for(size_t i = 0; i < Joints::numOfJoints; i++)
      {
        //If the difference in the movement of a joint was steady but slow (under 3_deg), but for one frame it jumped (over 4_deg and different signs), we now that this joint sensor is defect
        if(std::fabs(difs[0].angles[i]) < 3_deg &&
           std::fabs(difs[1].angles[i]) > 4_deg &&
           std::fabs(difs[2].angles[i]) > 4_deg &&
           std::fabs(difs[3].angles[i]) < 3_deg &&
           std::signbit(float(difs[1].angles[i])) != std::signbit(float(difs[2].angles[i])))
          OUTPUT_TEXT("Broken Joint < " << TypeRegistry::getEnumName((static_cast<Joints::Joint>(i))) << " > " << "at Frame (Logframe/FrameInfo.time) " << frameCounter - 5 << " / " << theFrameInfo.time << " with value " << difs[1].angles[i] << " " << difs[2].angles[i]); //the frameInfo.time is needed as long as the log frame can be desynced.
      }
    }
    return true;
  });
  return finished;
}

bool LogExtractor::goThroughLog(const std::map<const MessageID, Streamable*>& representations, const std::function<bool(const std::string& frameType)>& executeAction)
{
  std::string frameType;
  bool filled = false;
  for(MessageQueue::Message message : logPlayer)
  {
    const MessageID id = logPlayer.id(message);
    auto repr = representations.find(id);
    if(repr != representations.end())
    {
      // Does not convert logs automatically now, can be found in LogDataProvider
      message.bin() >> *repr->second;
      filled = true;
    }
    else if(id == idFrameBegin)
    {
      message.bin() >> frameType;
      filled = false;
    }
    else if(id == idFrameFinished && filled)
    {
      if(!executeAction(frameType))
        return false;
    }
  }
  return true;
}
