/*
 *  Copyright (c) 2019 by flomesh.io
 *
 *  Unless prior written consent has been obtained from the copyright
 *  owner, the following shall not be allowed.
 *
 *  1. The distribution of any source codes, header files, make files,
 *     or libraries of the software.
 *
 *  2. Disclosure of any source codes pertaining to the software to any
 *     additional parties.
 *
 *  3. Alteration or removal of any notices in or on the software or
 *     within the documentation included within the software.
 *
 *  ALL SOURCE CODE AS WELL AS ALL DOCUMENTATION INCLUDED WITH THIS
 *  SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION, WITHOUT WARRANTY OF ANY
 *  KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef DEMUX_HPP
#define DEMUX_HPP

#include "filter.hpp"
#include "data.hpp"
#include "list.hpp"
#include "pipeline.hpp"

namespace pipy {

//
// QueueDemuxer
//

class QueueDemuxer : public EventFunction {
public:
  void reset();
  void isolate();
  void shutdown();

protected:
  QueueDemuxer() {}

  virtual auto on_new_sub_pipeline() -> Pipeline* = 0;

private:
  class Stream;

  List<Stream> m_streams;
  bool m_isolated = false;
  bool m_shutdown = false;

  void on_event(Event *evt) override;
  void flush();

  //
  // QueueDemuxer::Stream
  //

  class Stream :
    public pjs::Pooled<Stream>,
    public List<Stream>::Item,
    public EventSource
  {
    Stream(QueueDemuxer *demux);
    ~Stream();

    void data(Data *data);
    void end(MessageEnd *end);

    QueueDemuxer* m_demuxer;
    pjs::Ref<Pipeline> m_pipeline;
    pjs::Ref<MessageStart> m_start;
    pjs::Ref<MessageEnd> m_end;
    Data m_buffer;
    bool m_input_end = false;
    bool m_output_end = false;
    bool m_isolated = false;

    virtual void on_reply(Event *evt) override;

    friend class QueueDemuxer;
  };

  friend class Stream;
};

//
// DemuxQueue
//

class DemuxQueue : public Filter, public QueueDemuxer {
public:
  DemuxQueue();

private:
  DemuxQueue(const DemuxQueue &r);
  ~DemuxQueue();

  virtual auto clone() -> Filter* override;
  virtual void chain() override;
  virtual void reset() override;
  virtual void process(Event *evt) override;
  virtual void shutdown() override;
  virtual void dump(std::ostream &out) override;

  virtual auto on_new_sub_pipeline() -> Pipeline* override;
};

//
// Demux
//

class Demux :
  public Filter,
  public EventSource
{
public:
  Demux();

private:
  Demux(const Demux &r);
  ~Demux();

  virtual auto clone() -> Filter* override;
  virtual void reset() override;
  virtual void process(Event *evt) override;
  virtual void dump(std::ostream &out) override;

  pjs::Ref<Pipeline> m_pipeline;
};

} // namespace pipy

#endif // DEMUX_HPP
