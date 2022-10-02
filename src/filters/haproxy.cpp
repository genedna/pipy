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

#include "haproxy.hpp"
#include "data.hpp"
#include "pipeline.hpp"
#include "module.hpp"
#include "log.hpp"

namespace pipy {
namespace haproxy {

static pjs::ConstStr s_TCP4("TCP4");
static pjs::ConstStr s_TCP6("TCP6");
static pjs::ConstStr s_UDP4("UDP4");
static pjs::ConstStr s_UDP6("UDP6");
static pjs::ConstStr s_UNIX("UNIX");
static pjs::ConstStr s_UNIX_DGRAM("UNIX_DGRAM");
static pjs::ConstStr s_UNKNOWN("UNKNOWN");
static pjs::ConstStr s_LOCAL("LOCAL");
static pjs::ConstStr s_PROXY("PROXY");
static pjs::ConstStr s_command("command");
static pjs::ConstStr s_protocol("protocol");
static pjs::ConstStr s_sourceAddress("sourceAddress");
static pjs::ConstStr s_sourcePort("sourcePort");
static pjs::ConstStr s_targetAddress("targetAddress");
static pjs::ConstStr s_targetPort("targetPort");

//
// Server
//

Server::Server(pjs::Function *on_connect)
  : m_on_connect(on_connect)
{
}

Server::Server(const Server &r)
  : Filter(r)
  , m_on_connect(r.m_on_connect)
{
}

Server::~Server()
{
}

void Server::dump(Dump &d) {
  Filter::dump(d);
  d.name = "acceptHAProxy";
}

auto Server::clone() -> Filter* {
  return new Server(*this);
}

void Server::reset() {
  Filter::reset();
  m_pipeline = nullptr;
  m_version = 0;
  m_header_read_ptr = 0;
  m_header_read_chr = 0;
  m_address_size_v2 = 0;
  m_error = false;
}

void Server::process(Event *evt) {
  if (auto data = evt->as<Data>()) {
    if (!m_pipeline && !m_error) {
      Data out;
      data->shift_to(
        [this](int c) {
          if (m_version == 1) {
            if (c == '\n' && m_header_read_chr == '\r') {
              m_header[m_header_read_ptr - 1] = ' ';
              m_header[m_header_read_ptr] = '\0';
              parse_header_v1();
              return true;
            }
            if (m_header_read_ptr >= 108) {
              error();
              return true;
            }
          }

          if (m_header_read_ptr < sizeof(m_header)) {
            m_header[m_header_read_ptr] = c;
          }

          m_header_read_chr = c;
          m_header_read_ptr++;

          if (!m_version) {
            if (m_header_read_ptr == 6) {
              if (!std::memcmp(m_header, "PROXY ", 6)) {
                m_version = 1;
              }
            } else if (m_header_read_ptr == 12) {
              if (!std::memcmp(m_header, "\r\n\r\n\0\r\nQUIT\n", 12)) {
                m_version = 2;
              }
            } else if (m_header_read_ptr > 16) {
              error();
              return true;
            }

          } else if (m_version == 2) {
            if (m_header_read_ptr == 16) {
              m_address_size_v2 = (
                (((uint16_t)m_header[14] & 0xff) << 8) |
                (((uint16_t)m_header[15] & 0xff) << 0)
              );
            }
            if (m_header_read_ptr == 16 + m_address_size_v2) {
              parse_header_v2();
              return true;
            }
          }
          return false;
        },
        out
      );
    }
    if (m_pipeline) {
      Filter::output(data, m_pipeline->input());
    }
  }
}

void Server::parse_header_v1() {
  auto *p = m_header + 6;

  pjs::Ref<pjs::Str> protocol;
  if (!std::memcmp(p, "TCP4 ", 5)) { protocol = s_TCP4; p += 5; } else
  if (!std::memcmp(p, "TCP6 ", 5)) { protocol = s_TCP6; p += 5; } else
  if (!std::memcmp(p, "UNKNOWN ", 8)) { protocol = s_UNKNOWN; p += 8; }
  else { error(); return; }

  auto next_field = [](char **ptr) -> char* {
    auto n = 0;
    auto p = *ptr;
    while (p[n] && p[n] != ' ') n++;
    if (!n) return nullptr;
    p[n] = '\0';
    *ptr = p + n + 1;
    return p;
  };

  pjs::Value obj(pjs::Object::make());
  obj.o()->set(s_protocol, protocol.get());

  if (protocol != s_UNKNOWN) {
    uint8_t ip[16];
    const char *s[4];

    for (int i = 0; i < 4; i++) {
      if (!(s[i] = next_field(&p))) {
        error();
        return;
      }
    }

    if (protocol == s_TCP6) {
      if (!utils::get_ip_v6(s[0], ip) || !utils::get_ip_v6(s[1], ip)) {
        error();
        return;
      }
    } else {
      if (!utils::get_ip_v4(s[0], ip) || !utils::get_ip_v4(s[1], ip)) {
        error();
        return;
      }
    }

    obj.o()->set(s_sourceAddress, pjs::Str::make(s[0], s[1] - s[0] - 1));
    obj.o()->set(s_targetAddress, pjs::Str::make(s[1], s[2] - s[1] - 1));
    obj.o()->set(s_sourcePort, std::atoi(s[2]));
    obj.o()->set(s_targetPort, std::atoi(s[3]));
  }

  start(obj);
}

void Server::parse_header_v2() {
  auto version = (m_header[12] >> 4) & 0x0f;
  auto command = (m_header[12] >> 0) & 0x0f;

  if (version != 2) {
    error();
    return;
  }

  pjs::Value obj(pjs::Object::make());

  switch (command) {
    case 0: obj.o()->set(s_command, s_LOCAL.get()); break;
    case 1: obj.o()->set(s_command, s_PROXY.get()); break;
    default: error(); return;
  }

  bool is_ipv6 = false;
  bool is_unix = false;

  switch (m_header[13]) {
    case 0x00: obj.o()->set(s_protocol, s_UNKNOWN.get()); break;
    case 0x11: obj.o()->set(s_protocol, s_TCP4.get()); break;
    case 0x12: obj.o()->set(s_protocol, s_UDP4.get()); break;
    case 0x21: obj.o()->set(s_protocol, s_TCP6.get()); is_ipv6 = true; break;
    case 0x22: obj.o()->set(s_protocol, s_TCP6.get()); is_ipv6 = true; break;
    case 0x31: obj.o()->set(s_protocol, s_UNIX.get()); is_unix = true; break;
    case 0x32: obj.o()->set(s_protocol, s_UNIX_DGRAM.get()); is_unix = true; break;
    default: error(); return;
  }

  if (is_ipv6) {

  } else if (is_unix) {

  } else {
    auto p = (const uint8_t*)(m_header + 16);
    char src_addr[100];
    char dst_addr[100];
    auto src_addr_size = std::snprintf(src_addr, sizeof(src_addr), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
    auto dst_addr_size = std::snprintf(dst_addr, sizeof(dst_addr), "%d.%d.%d.%d", p[4], p[5], p[6], p[7]);
    auto src_port = ((int)p[ 8] << 8) | p[ 9];
    auto dst_port = ((int)p[10] << 8) | p[11];
    obj.o()->set(s_sourceAddress, pjs::Str::make(src_addr, src_addr_size));
    obj.o()->set(s_targetAddress, pjs::Str::make(dst_addr, dst_addr_size));
    obj.o()->set(s_sourcePort, src_port);
    obj.o()->set(s_targetPort, dst_port);
  }

  start(obj);
}

void Server::start(pjs::Value &obj) {
  pjs::Value ret;
  if (!Filter::callback(m_on_connect, 1, &obj, ret) || !ret.to_boolean()) {
    error();
    return;
  }

  m_pipeline = Filter::sub_pipeline(0, false, Filter::output());
}

void Server::error() {
  Filter::output(StreamEnd::make());
  m_error = true;
}

//
// Client
//

static Data::Producer s_dp("connectHAProxy");

Client::Client(const pjs::Value &target)
  : m_target(target)
{
}

Client::Client(const Client &r)
  : Filter(r)
  , m_target(r.m_target)
{
}

Client::~Client()
{
}

void Client::dump(Dump &d) {
  Filter::dump(d);
  d.name = "connectHAProxy";
}

auto Client::clone() -> Filter* {
  return new Client(*this);
}

void Client::reset() {
  Filter::reset();
  m_pipeline = nullptr;
}

void Client::process(Event *evt) {
}

} // namespace haproxy
} // namespace pipy