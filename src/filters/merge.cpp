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

#include "merge.hpp"
#include "pipeline.hpp"
#include "logging.hpp"

namespace pipy {

//
// Merge
//

Merge::Merge(const pjs::Value &key, pjs::Object *options)
  : MuxBase(key, options)
{
}

Merge::Merge(const Merge &r)
  : MuxBase(r)
{
}

Merge::~Merge() {
}

void Merge::dump(std::ostream &out) {
  out << "merge";
}

auto Merge::clone() -> Filter* {
  return new Merge(*this);
}

void Merge::process(Event *evt) {
  MuxBase::process(evt->clone());
  output(evt);
}

auto Merge::on_new_session() -> MuxBase::Session* {
  return new Session();
}

//
// Merge::Session
//

auto Merge::Session::open_stream() -> EventFunction* {
  return new Stream(this);
}

void Merge::Session::close_stream(EventFunction *stream) {
  delete static_cast<Stream*>(stream);
}

//
// Merge::Stream
//

void Merge::Stream::on_event(Event *evt) {
  if (auto start = evt->as<MessageStart>()) {
    if (!m_start) {
      m_start = start;
    }

  } else if (auto data = evt->as<Data>()) {
    if (m_start) {
      m_buffer.push(*data);
    }

  } else if (evt->is<MessageEnd>() || evt->is<StreamEnd>()) {
    if (m_start) {
      auto inp = m_session->input();
      inp->input(m_start);
      if (!m_buffer.empty()) {
        inp->input(Data::make(m_buffer));
        m_buffer.clear();
      }
      inp->input(MessageEnd::make());
    }
  }
}

} // namespace pipy
