import { defineUserConfig } from "vuepress";
import { slimsearchPlugin } from '@vuepress/plugin-slimsearch'

import theme from "./theme.js";

export default defineUserConfig({
  base: "/",

  lang: "zh-CN",
  title: '远岫文档',
  description: 'distpeak-book',

  theme,
  
  plugins: [
    slimsearchPlugin({
      // 配置项
    }),
  ],
});
