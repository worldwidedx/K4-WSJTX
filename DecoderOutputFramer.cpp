#include "DecoderOutputFramer.hpp"

#include <QIODevice>

void DecoderOutputFramer::drain (QIODevice& device,
                                 EventHandler const& handler)
{
  while (device.canReadLine ())
    {
      auto const rawLine = device.readLine ();
      Event event {EventType::Malformed, generation_, rawLine, {}};

      if (rawLine.startsWith ("<DecoderReady")
          || rawLine.startsWith ("<DecoderError"))
        {
          auto line = rawLine;
          if (line.endsWith ('\n')) line.chop (1);
          if (line.endsWith ('\r')) line.chop (1);
          QByteArray const readyPrefix {"<DecoderReady> version="};
          QByteArray const errorPrefix {"<DecoderError> status="};
          if (line.startsWith (readyPrefix))
            {
              auto const versionText = line.mid (readyPrefix.size ());
              bool valid = !versionText.isEmpty ();
              for (auto const character : versionText)
                valid = valid && character >= '0' && character <= '9';
              bool numeric {false};
              auto const version = versionText.toInt (&numeric);
              if (valid && numeric && version > 0)
                {
                  event.type = EventType::Ready;
                  event.protocolVersion = version;
                }
            }
          else if (line.startsWith (errorPrefix))
            {
              auto const status = line.mid (errorPrefix.size ());
              bool valid = !status.isEmpty ();
              for (auto const character : status)
                valid = valid && ((character >= 'a' && character <= 'z')
                                  || (character >= '0' && character <= '9')
                                  || character == '-');
              if (valid)
                {
                  event.type = EventType::Error;
                  event.errorStatus = status;
                }
            }
        }
      else if (rawLine.startsWith ("<DecodeStarted>"))
        {
          qint32 generation {0};
          if (!generation_ && DecoderIpc::parseStart (rawLine, &generation))
            {
              generation_ = generation;
              event.type = EventType::Started;
              event.generation = generation;
            }
        }
      else if (rawLine.startsWith ("<DecodeRejected"))
        {
          if (generation_ && rawLine.startsWith ("<DecodeRejected> "))
            event.type = EventType::Rejected;
        }
      else if (rawLine.startsWith ("<DecodeFinished>"))
        {
          DecoderIpc::Completion completion {};
          if (DecoderIpc::parseCompletion (rawLine, &completion)
              && generation_ == completion.generation)
            {
              event.type = EventType::Finished;
              event.generation = completion.generation;
              event.completion = completion;
              generation_ = 0;
            }
        }
      else if (generation_)
        {
          event.type = EventType::Record;
        }

      if (handler) handler (event);
    }
}

void DecoderOutputFramer::reset ()
{
  generation_ = 0;
}

qint32 DecoderOutputFramer::currentGeneration () const
{
  return generation_;
}
