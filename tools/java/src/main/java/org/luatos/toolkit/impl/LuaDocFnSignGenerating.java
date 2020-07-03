package org.luatos.toolkit.impl;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.luatos.toolkit.api.FnSignGenerating;
import org.luatos.toolkit.bean.FnExample;
import org.luatos.toolkit.bean.FnParam;
import org.luatos.toolkit.bean.FnSign;
import org.nutz.lang.Lang;
import org.nutz.lang.Strings;
import org.nutz.lang.util.Regex;

public class LuaDocFnSignGenerating implements FnSignGenerating {

    private static final int IN_SUMMARY = 0;
    private static final int IN_FUNC = 1;
    private static final int IN_EXAMPLE = 2;

    private static String _r2 = "^\\s*([\\w\\d.]+)\\s*(\\(([^)]*)\\))?.*$";
    private static Pattern P2 = Regex.getPattern(_r2);

    @Override
    public FnSign gen(String block) {
        String[] lines = block.split("\r?\n");
        List<String> summaries = new ArrayList<>(lines.length);
        String func;
        String[] pmnms = null;
        List<FnParam> params = new ArrayList<>(lines.length);
        FnExample exmLast = null;

        String feType = null; // @return @string ...
        String feCmt = "";

        // 准备返回值
        FnSign fn = new FnSign();

        // 三种状态
        int mode = IN_SUMMARY;

        // 逐行搞
        for (String line : lines) {
            // 还在摘要部分
            if (IN_SUMMARY == mode) {
                // 进入函数部分了
                if (line.startsWith("@function")) {
                    mode = IN_FUNC;
                    func = line.substring("@function".length()).trim();
                    // 来，分析一下
                    Matcher m = P2.matcher(func);
                    if (!m.find()) {
                        throw Lang.makeThrow("Invalid Lua func", func);
                    }
                    fn.setName(m.group(1));
                    pmnms = Strings.splitIgnoreBlank(m.group(3));
                    continue;
                }
                // 计入摘要
                summaries.add(line);
            }
            // 进入函数部分
            else if (IN_FUNC == mode) {
                // 明确的开始例子部分
                if (line.trim().equals("@usage")) {
                    mode = IN_EXAMPLE;
                }
                // 进入例子部分了
                else if (line.startsWith("--")) {
                    if (null != exmLast && exmLast.hasCode()) {
                        fn.addExamples(exmLast);
                    }

                    exmLast = new FnExample();
                    exmLast.appendSummary(line.substring(2).trim());

                    mode = IN_EXAMPLE;
                }
                // 开始参数或者返回
                else if (line.startsWith("@")) {
                    // 推入旧的
                    if (!Strings.isBlank(feType)) {
                        pushFnEntity(fn, params, feType, feCmt);
                    }
                    // 开启新的
                    int pos = line.indexOf(' ', 1);
                    if (pos > 0) {
                        feType = line.substring(1, pos).trim();
                        feCmt = line.substring(pos + 1).trim();
                    } else {
                        feType = "??";
                        feCmt = line.substring(1).trim();
                    }
                }
            }
            // 进入例子部分
            else if (IN_EXAMPLE == mode) {
                // 追加说明
                if (line.startsWith("--")) {
                    if (null != exmLast && exmLast.hasCode()) {
                        fn.addExamples(exmLast);
                    }
                    if (null == exmLast) {
                        exmLast = new FnExample();
                    }
                    exmLast.appendSummary(line.substring(2).trim());
                }
                // 那就是例子代码咯
                else {
                    exmLast.appendCode(line);
                }
            }
        }

        // 推入最后一个实体
        if (!Strings.isBlank(feType)) {
            pushFnEntity(fn, params, feType, feCmt);
        }

        // 函数的摘要
        fn.setSummary(Strings.join(System.lineSeparator(), summaries));

        // 归纳函数参数
        int plMax = Math.max(params.size(), pmnms.length);
        for (int i = 0; i < plMax; i++) {
            String pmnm = i < pmnms.length ? pmnms[i] : "?";
            FnParam param = i < params.size() ? params.get(i) : null;
            // 看来只有形参名，木有给定注释说明
            if (null == param) {
                param = new FnParam();
            }
            param.setName(pmnm);
            fn.addParams(param);
        }

        // 最后一个例子
        if (null != exmLast && exmLast.hasCode()) {
            fn.addExamples(exmLast);
        }

        // 搞定
        return fn;
    }

    private void pushFnEntity(FnSign fn, List<FnParam> params, String feType, String feCmt) {
        // 返回值
        if ("return".equals(feType)) {
            int pos = feCmt.indexOf(' ');
            if (pos > 0) {
                fn.setReturnType(feCmt.substring(0, pos).trim());
                fn.setReturnComment(feCmt.substring(pos + 1).trim());
            } else {
                fn.setReturnType("??");
                fn.setReturnComment(feCmt);
            }
        }
        // 参数
        else {
            FnParam param = new FnParam();
            param.setType(feType);
            param.setComment(feCmt);
            params.add(param);
        }
    }

}