#include <ATen/TracerMode.h>
#include <ATen/core/op_registration/op_registration.h>
#include <c10/core/ScalarType.h>
#include <c10/util/Optional.h>
#include <torch/csrc/jit/frontend/tracer.h>
#include <torch/csrc/utils/memory.h>
#include <torch/library.h>

using namespace at;

namespace torch { namespace TraceType {

namespace {

Tensor & copy_(Tensor & self, const Tensor & src, bool non_blocking) {
  jit::Value* output = nullptr;
#if !defined(PYTORCH_DISABLE_TRACING)
  if(torch::jit::tracer::isTracing()) {
    const jit::tracer::TracingState& state = *jit::tracer::getTracingState();
    auto& graph = state.graph;
    if (state.force_outplace && self.storage().use_count() <= 1) {
      // if you have no views of self, then an in place copy is equivalent to
      // making sure we expand src to the same size as self
      jit::Node* node = graph->create(jit::aten::expand_as, /*num_outputs=*/1);
      jit::tracer::addInputs(node, "src", src);
      jit::tracer::addInputs(node, "self", self);
      graph->insertNode(node);
      output = node->output();
    } else {
      output = graph->insert(
          jit::aten::copy_,
          {jit::tracer::getValueTrace(self), jit::tracer::getValueTrace(src)});
      jit::tracer::recordSourceLocation(output->node());
    }
    jit::tracer::ensureUniqueIfOutOfPlaced("copy_ (possibly due to an assignment)", self);
  }
#endif

  static auto op = c10::Dispatcher::singleton()
      .findSchemaOrThrow("aten::copy_", "")
      .typed<Tensor & (Tensor &, const Tensor &, bool)>();
  {
    at::tracer::impl::NoTracerDispatchMode tracer_guard;
    c10::Dispatcher::singleton()
        .redispatch<Tensor &, Tensor &, const Tensor &, bool>(
            op, c10::DispatchKey::Tracer, self, src, non_blocking);
  }

#if !defined(PYTORCH_DISABLE_TRACING)
  if(torch::jit::tracer::isTracing()) {
    jit::tracer::setOutput(output, self);
  }
#endif
  return self;
}

Tensor& resize_(
    Tensor& self,
    IntArrayRef size,
    c10::optional<MemoryFormat> optional_memory_format) {
#if !defined(PYTORCH_DISABLE_TRACING)
  if (torch::jit::tracer::isTracing()) {
    jit::tracer::ArgumentStash::popIntArrayRef("size");
    jit::tracer::warn("resize_", jit::tracer::WARN_RESIZE);
    jit::tracer::delValueTrace(self);
  }
#endif

  static auto op = c10::Dispatcher::singleton()
      .findSchemaOrThrow("aten::resize_", "")
      .typed<Tensor & (Tensor &, IntArrayRef, c10::optional<MemoryFormat>)>();
  {
    at::tracer::impl::NoTracerDispatchMode tracer_guard;
    c10::Dispatcher::singleton()
        .redispatch<Tensor &, Tensor &, IntArrayRef, c10::optional<MemoryFormat>>(
            op, c10::DispatchKey::Tracer, self, size, std::move(optional_memory_format));
  }

  return self;
}

Tensor& resize_as_(
    Tensor& self,
    const Tensor& the_template,
    c10::optional<MemoryFormat> optional_memory_format) {
#if !defined(PYTORCH_DISABLE_TRACING)
  if (torch::jit::tracer::isTracing()) {
    jit::tracer::warn("resize_as_", jit::tracer::WARN_RESIZE);
    jit::tracer::delValueTrace(self);
  }
#endif

  static auto op = c10::Dispatcher::singleton()
      .findSchemaOrThrow("aten::resize_as_", "")
      .typed<Tensor & (Tensor &, const Tensor &, c10::optional<MemoryFormat>)>();
  {
    at::tracer::impl::NoTracerDispatchMode tracer_guard;
    c10::Dispatcher::singleton()
        .redispatch<Tensor &, Tensor &, const Tensor &, c10::optional<MemoryFormat>>(
            op, c10::DispatchKey::Tracer, self, the_template, std::move(optional_memory_format));
  }
  return self;
}

Tensor detach(const Tensor & self) {
#if !defined(PYTORCH_DISABLE_TRACING)
  torch::jit::Node* node = nullptr;
  if (jit::tracer::isTracing()) {
    auto& graph = jit::tracer::getTracingState()->graph;
    node = graph->create(jit::aten::detach, /*num_outputs=*/0);
    jit::tracer::recordSourceLocation(node);
    jit::tracer::addInputs(node, "self", self);
    graph->insertNode(node);
  }
#endif

  static auto op = c10::Dispatcher::singleton()
      .findSchemaOrThrow("aten::detach", "")
      .typed<Tensor (const Tensor &)>();
  auto result = [&]() {
    at::tracer::impl::NoTracerDispatchMode tracer_guard;
    return c10::Dispatcher::singleton()
        .redispatch<Tensor, const Tensor &>(op, c10::DispatchKey::Tracer, self);
  }();

#if !defined(PYTORCH_DISABLE_TRACING)
  if (jit::tracer::isTracing()) {
    jit::tracer::addOutput(node, result);
  }
#endif
  return result;
}

Tensor & detach_(Tensor & self) {
#if !defined(PYTORCH_DISABLE_TRACING)
  torch::jit::Node* node = nullptr;
  if (jit::tracer::isTracing()) {
    auto& graph = jit::tracer::getTracingState()->graph;
    node = graph->create(jit::aten::detach, /*num_outputs=*/0);
    jit::tracer::recordSourceLocation(node);
    jit::tracer::addInputs(node, "self", self);
    graph->insertNode(node);
    jit::tracer::ensureUniqueIfOutOfPlaced("detach_", self);
  }
#endif

  static auto op = c10::Dispatcher::singleton()
      .findSchemaOrThrow("aten::detach_", "")
      .typed<Tensor & (Tensor &)>();
  {
    at::tracer::impl::NoTracerDispatchMode tracer_guard;
    c10::Dispatcher::singleton()
        .redispatch<Tensor &, Tensor &>(op, c10::DispatchKey::Tracer, self);
  }

#if !defined(PYTORCH_DISABLE_TRACING)
  if (jit::tracer::isTracing()) {
    jit::tracer::addOutput(node, self);
  }
#endif
  return self;
}

TORCH_LIBRARY_IMPL(aten, Tracer, m) {
  m.impl_UNBOXED("resize_", resize_);
  m.impl_UNBOXED("resize_as_", resize_as_);
  m.impl("detach", TORCH_FN(detach));
  m.impl_UNBOXED("detach_", detach_);
  m.impl_UNBOXED("copy_", copy_);

  // Skip tracing for the following ops by registering fallthrough kernel explicitly.
  m.impl("backward", CppFunction::makeFallthrough());
  m.impl("set_data", CppFunction::makeFallthrough());
  m.impl("data", CppFunction::makeFallthrough());
  m.impl("is_leaf", CppFunction::makeFallthrough());
  m.impl("output_nr", CppFunction::makeFallthrough());
  m.impl("_version", CppFunction::makeFallthrough());
  m.impl("requires_grad_", CppFunction::makeFallthrough());
  m.impl("retain_grad", CppFunction::makeFallthrough());
}

}  // namespace

}} // namespace torch::TraceType
