# W1 任务结果模板

每个并行实现对话只新增自己的 `W1-Tx-RESULT.md`，不要编辑其他任务结果或总交接。

```markdown
# W1-Tx Result

## Identity
- Task:
- Branch:
- Base commit: 927001a772a4431ae1cf74b745d9abdb884cd336
- Final commit(s):

## Scope
- Implemented:
- Explicitly not implemented:
- Files changed:

## Contract
- Public types/functions:
- Units/capacities/versions:
- Error and edge-case semantics:

## Verification
- Commands:
- GCC:
- Clang:
- MSVC:
- Coverage:
- git diff --check:

## Integration Request
- Shared build files to update:
- Runtime/event/UI/platform adapters needed:
- Expected source/test lists:

## Risks
- Unproven behavior:
- Preserved failures:
- Claims that must not be made:
```

没有实际运行的验证必须写 `not run` 和原因，不能写成 passed。
