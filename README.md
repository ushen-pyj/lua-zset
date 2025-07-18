key和score都是整数类型, 需要自己为元素提供唯一id, 避免精度问题score也用整数

1. **加载模块**

```lua
local zset = require "zset"
```

2. **创建有序集合**

```lua
local zs = zset.create()  -- 默认升序
-- local zs = zset.create(1)  -- 传1为降序
```

3. **插入或更新元素分数**

```lua
zs:update(1001, 88)  -- 插入key为1001，分数为88
zs:update(1002, 77)
zs:update(1001, 99)  -- 更新1001的分数
```

4. **获取元素分数**

```lua
local score = zs:getscore(1001)
print(score)  -- 输出99
```

5. **删除元素**

```lua
zs:delete(1002)
```

6. **范围查询**

```lua
local res = zs:range(0, 9)  -- 获取第1~10名
for i, v in ipairs(res) do
    print(v.key, v.score, v.rank)
end
```
