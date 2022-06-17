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

#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "pjs/pjs.hpp"
#include "event.hpp"
#include "input.hpp"
#include "output.hpp"
#include "list.hpp"

#include <list>
#include <memory>
#include <set>

namespace pipy {

class ModuleBase;
class Pipeline;
class Filter;
class Context;
class InputContext;

//
// PipelineLayout
//

class PipelineLayout :
  public pjs::RefCount<PipelineLayout>,
  public List<PipelineLayout>::Item
{
public:
  enum Type {
    NAMED,
    LISTEN,
    READ,
    TASK,
  };

  static auto make(ModuleBase *module, Type type, const std::string &name) -> PipelineLayout* {
    return new PipelineLayout(module, type, -1, name);
  }

  static auto make(ModuleBase *module, Type type, int index, const std::string &name) -> PipelineLayout* {
    return new PipelineLayout(module, type, index, name);
  }

  static void for_each(std::function<void(PipelineLayout*)> callback) {
    for (auto *p = s_all_pipeline_layouts.head(); p; p = p->next()) {
      callback(p);
    }
  }

  auto module() const -> ModuleBase* { return m_module; }
  auto index() const -> int { return m_index; }
  auto type() const -> Type { return m_type; }
  auto name() const -> pjs::Str* { return m_name; }
  auto allocated() const -> size_t { return m_allocated; }
  auto active() const -> size_t { return m_pipelines.size(); }
  auto append(Filter *filter) -> Filter*;
  void bind();
  void shutdown();

private:
  PipelineLayout(ModuleBase *module, Type type, int index, const std::string &name);
  ~PipelineLayout();

  auto alloc(Context *ctx) -> Pipeline*;
  void free(Pipeline *pipeline);

  Type m_type;
  int m_index;
  pjs::Ref<pjs::Str> m_name;
  std::list<std::unique_ptr<Filter>> m_filters;
  pjs::Ref<ModuleBase> m_module;
  Pipeline* m_pool = nullptr;
  List<Pipeline> m_pipelines;
  size_t m_allocated = 0;

  static List<PipelineLayout> s_all_pipeline_layouts;

  friend class pjs::RefCount<PipelineLayout>;
  friend class Pipeline;
  friend class Graph;
};

//
// PipelineBase
//

class PipelineBase : public AutoReleased, public EventFunction
{
};

//
// Pipeline
//

class Pipeline :
  public PipelineBase,
  public List<Pipeline>::Item
{
public:
  static auto make(PipelineLayout *layout, Context *ctx) -> Pipeline* {
    return layout->alloc(ctx);
  }

  auto layout() const -> PipelineLayout* { return m_layout; }
  auto context() const -> Context* { return m_context; }
  auto output() const -> Output* { return m_output; }
  void output(Output *output) { m_output = output; }

  virtual void chain(Input *input) override;

private:
  Pipeline(PipelineLayout *layout);
  ~Pipeline();

  virtual void on_event(Event *evt) override;
  virtual void on_recycle() override;

  PipelineLayout* m_layout;
  Pipeline* m_next_free = nullptr;
  List<Filter> m_filters;
  pjs::Ref<Context> m_context;
  pjs::Ref<Output> m_output;

  void shutdown();
  void reset();

  friend class pjs::RefCount<Pipeline>;
  friend class PipelineLayout;
  friend class Filter;
};

} // namespace pipy

#endif // PIPELINE_HPP
