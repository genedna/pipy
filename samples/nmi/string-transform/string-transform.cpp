#include <pipy/nmi-cpp.h>

using namespace pipy;

thread_local static nmi::Variable s_str_map;

class StringTransformPipeline : public nmi::PipelineBase {
public:
  using nmi::PipelineBase::PipelineBase;

  void process(nmi::Local evt) {
    if (evt.is<nmi::Data>()) {
      auto data = evt.as<nmi::Data>();
      while (data.size() > 0) {
        char buf[0x10000];
        auto d = data.shift(std::min(data.size(), sizeof(buf)));
        auto n = d.read(buf, sizeof(buf));
        scan(buf, n);
      }
    } else {
      flush();
      nmi::PipelineBase::output(evt);
    }
  }

private:
  enum State {
    NORMAL,
    LINE_COMMENT,
    BLOCK_COMMENT,
    REGEXP_MAYBE,
    REGEXP,
  };

  void scan(const char *buf, int len) {
    for (auto i = 0; i < len; i++) {
      auto c = buf[i];
      if (m_current_quote) {
        if (m_has_escaped) {
          switch (c) {
            case 'a': c = '\a'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'v': c = '\v'; break;
            case '0': c = '\0'; break;
            case 'x': c = '\x10'; break;
            case 'u': c = '\x11'; break;
          }
          m_current_string += c;
          m_has_escaped = false;
        } else if (c == m_current_quote) {
          output(m_current_string);
          output(c);
          m_current_string.clear();
          m_current_quote = 0;
        } else if (c == '\\') {
          m_has_escaped = true;
        } else {
          m_current_string += c;
        }
      } else {
        output(c);
        if (m_state == LINE_COMMENT) {
          if (c == '\n') {
            m_state = NORMAL;
          }
        } else if (m_state == BLOCK_COMMENT) {
          if (c == '/' && m_last_char == '*') {
            m_state = NORMAL;
          }
        } else if (m_state == REGEXP) {
          if (m_has_escaped) {
            m_has_escaped = false;
          } else if (c == '\\') {
            m_has_escaped = true;
          } else if (c == '/') {
            m_state = NORMAL;
          }
        } else if (c == '/') {
          if (m_last_char == '/') {
            m_state = LINE_COMMENT;
          } else if (
            m_last_non_space != '_' &&
            m_last_non_space != ')' &&
            m_last_non_space != ']' &&
            !std::isalnum(m_last_non_space)
          ) {
            m_state = REGEXP_MAYBE;
          }
        } else if (c == '*') {
          if (m_last_char == '/') {
            m_state = BLOCK_COMMENT;
          }
        } else if (m_state == REGEXP_MAYBE) {
          m_state = REGEXP;
          m_has_escaped = (c == '\\');
        } else if (c == '"' || c == '\'') {
          m_current_quote = c;
        }
        m_last_char = c;
        if (!std::isspace(c)) m_last_non_space = c;
      }
    }
  }

  void output(char c) {
    m_output_buffer[m_output_pointer++] = c;
    if (m_output_pointer >= sizeof(m_output_buffer)) {
      flush();
    }
  }

  void output(std::string s) {
    auto m = s_str_map.get(this);
    if (m.is_object()) {
      auto v = m.as_object().get(nmi::String(s));
      if (!v.is_nullish()) {
        s = v.to_string().utf8_data();
      }
    }
    for (auto c : s) {
      if (c == m_current_quote) {
        output('\\');
        output(c);
      } else {
        switch (c) {
          case '\0': output('\\'); output('0'); break;
          case '\\': output('\\'); output('\\'); break;
          case '\a': output('\\'); output('a'); break;
          case '\b': output('\\'); output('b'); break;
          case '\f': output('\\'); output('f'); break;
          case '\n': output('\\'); output('n'); break;
          case '\r': output('\\'); output('r'); break;
          case '\t': output('\\'); output('t'); break;
          case '\v': output('\\'); output('v'); break;
          case '\x10': output('\\'); output('x'); break;
          case '\x11': output('\\'); output('u'); break;
          default: output(c); break;
        }
      }
    }
  }

  void flush() {
    if (m_output_pointer > 0) {
      nmi::PipelineBase::output(nmi::Data(m_output_buffer, m_output_pointer));
      m_output_pointer = 0;
    }
  }

  char m_output_buffer[1000];
  int m_output_pointer = 0;
  int m_current_quote = 0;
  std::string m_current_string;
  State m_state = NORMAL;
  char m_last_char = 0;
  char m_last_non_space = 0;
  bool m_has_escaped = false;
};

extern "C" void pipy_module_init() {
  s_str_map.define("__stringMap", "string-transform", nmi::Object());
  nmi::PipelineTemplate<StringTransformPipeline>::define();
}
