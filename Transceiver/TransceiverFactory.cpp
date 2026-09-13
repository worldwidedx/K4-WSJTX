#include "TransceiverFactory.hpp"
#include "TxInhibitTransceiver.hpp"

#include <stdexcept>

#include <QMetaType>

#include "K4RemoteTransceiver.hpp"
#include "moc_TransceiverFactory.cpp"

// Keep the upstream sentinel intact.  It is used throughout WSJT-X to mean
// "no transceiver", even though this fork does not expose that choice in the
// configuration UI.
char const * const TransceiverFactory::basic_transceiver_name_ = "None";

namespace
{
  enum {NonHamlibBaseId = 99899, K4RemoteId};
}

TransceiverFactory::TransceiverFactory ()
  : logger_ (boost::log::keywords::channel = "RIGCTRL")
{
  // The K4 remote protocol is the sole radio interface in this fork.
  K4RemoteTransceiver::register_transceiver (&transceivers_, K4RemoteId);
}

TransceiverFactory::~TransceiverFactory () = default;

auto TransceiverFactory::supported_transceivers () const -> Transceivers const&
{
  return transceivers_;
}

auto TransceiverFactory::CAT_port_type (QString const& name) const -> Capabilities::PortType
{
  return supported_transceivers ().value (name).port_type_;
}

bool TransceiverFactory::has_CAT_PTT (QString const& name) const
{
  auto const capabilities = supported_transceivers ().value (name);
  return capabilities.has_CAT_PTT_ || capabilities.model_number_ > NonHamlibBaseId;
}

bool TransceiverFactory::has_CAT_PTT_mic_data (QString const& name) const
{
  return supported_transceivers ().value (name).has_CAT_PTT_mic_data_;
}

bool TransceiverFactory::has_CAT_indirect_serial_PTT (QString const& name) const
{
  return supported_transceivers ().value (name).has_CAT_indirect_serial_PTT_;
}

bool TransceiverFactory::has_asynchronous_CAT (QString const& name) const
{
  return supported_transceivers ().value (name).asynchronous_;
}

std::unique_ptr<Transceiver> TransceiverFactory::create (ParameterPack const& params,
                                                        QThread * target_thread)
{
  if (supported_transceivers ().value (params.rig_name).model_number_ != K4RemoteId)
    throw std::invalid_argument {"Only the Elecraft K4 Remote transceiver is available in this fork"};

  std::unique_ptr<Transceiver> result {new K4RemoteTransceiver {
    &logger_, params.k4_host, params.k4_port, params.k4_password,
    params.k4_tls, params.k4_tls_identity, params.k4_encode_mode,
    params.k4_streaming_latency, params.k4_calibrated_gain,
    params.k4_has_calibration, params.k4_calibration_radio_context,
    params.poll_interval & 0x7fff}};
  if (target_thread) result->moveToThread (target_thread);
  return result;
}

ENUM_QDATASTREAM_OPS_IMPL (TransceiverFactory, DataBits);
ENUM_QDATASTREAM_OPS_IMPL (TransceiverFactory, StopBits);
ENUM_QDATASTREAM_OPS_IMPL (TransceiverFactory, Handshake);
ENUM_QDATASTREAM_OPS_IMPL (TransceiverFactory, PTTMethod);
ENUM_QDATASTREAM_OPS_IMPL (TransceiverFactory, TXAudioSource);
ENUM_QDATASTREAM_OPS_IMPL (TransceiverFactory, SplitMode);

ENUM_CONVERSION_OPS_IMPL (TransceiverFactory, DataBits);
ENUM_CONVERSION_OPS_IMPL (TransceiverFactory, StopBits);
ENUM_CONVERSION_OPS_IMPL (TransceiverFactory, Handshake);
ENUM_CONVERSION_OPS_IMPL (TransceiverFactory, PTTMethod);
ENUM_CONVERSION_OPS_IMPL (TransceiverFactory, TXAudioSource);
ENUM_CONVERSION_OPS_IMPL (TransceiverFactory, SplitMode);
