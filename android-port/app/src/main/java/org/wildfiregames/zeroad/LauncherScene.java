// SPDX-License-Identifier: GPL-2.0-or-later
package org.wildfiregames.zeroad;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.view.View;
import android.view.ViewGroup;

/** Dark/gold, responsive composition shared in style with the Warzone launcher. */
final class LauncherScene extends ViewGroup {
    private final View hero, actions, footer;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Shader background, glow;

    LauncherScene(Context context, View hero, View actions, View footer) {
        super(context);
        this.hero = hero;
        this.actions = actions;
        this.footer = footer;
        setWillNotDraw(false);
        setPadding(dp(24), dp(18), dp(24), dp(12));
        addView(hero);
        addView(actions);
        addView(footer);
    }

    private int dp(float value) { return Math.round(value * getResources().getDisplayMetrics().density); }

    @Override protected void onMeasure(int widthSpec, int heightSpec) {
        int width = MeasureSpec.getSize(widthSpec);
        int available = Math.max(0, width - getPaddingLeft() - getPaddingRight());
        int unrestricted = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        hero.measure(MeasureSpec.makeMeasureSpec(Math.min(available, dp(700)), MeasureSpec.EXACTLY), unrestricted);
        actions.measure(MeasureSpec.makeMeasureSpec(Math.min(available, dp(340)), MeasureSpec.EXACTLY), unrestricted);
        footer.measure(MeasureSpec.makeMeasureSpec(Math.min(available, dp(440)), MeasureSpec.EXACTLY), unrestricted);
        int required = getPaddingTop() + hero.getMeasuredHeight() + dp(20)
                + actions.getMeasuredHeight() + dp(20) + footer.getMeasuredHeight() + getPaddingBottom();
        setMeasuredDimension(width, resolveSize(Math.max(dp(320), required), heightSpec));
    }

    private void placeCentered(View child, int top) {
        int left = (getWidth() - child.getMeasuredWidth()) / 2;
        child.layout(left, top, left + child.getMeasuredWidth(), top + child.getMeasuredHeight());
    }

    @Override protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        int footerTop = getHeight() - getPaddingBottom() - footer.getMeasuredHeight();
        int heroTop = Math.round(getHeight() * 0.34f) - hero.getMeasuredHeight() / 2;
        int latestHeroTop = footerTop - dp(40) - actions.getMeasuredHeight() - hero.getMeasuredHeight();
        heroTop = Math.max(getPaddingTop(), Math.min(heroTop, latestHeroTop));
        placeCentered(hero, heroTop);
        placeCentered(actions, heroTop + hero.getMeasuredHeight() + dp(20));
        placeCentered(footer, footerTop);
    }

    @Override protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        background = new LinearGradient(0, 0, width, height,
                new int[]{0xFF16212B, 0xFF0B121C, 0xFF171E26}, null, Shader.TileMode.CLAMP);
        glow = new RadialGradient(width * 0.5f, height * 0.30f, Math.max(1, width * 0.55f),
                new int[]{0x23EDBD68, 0x00EDBD68}, null, Shader.TileMode.CLAMP);
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(background);
        canvas.drawRect(0, 0, getWidth(), getHeight(), paint);
        paint.setShader(glow);
        canvas.drawRect(0, 0, getWidth(), getHeight(), paint);
        paint.setShader(null);
        paint.setStrokeWidth(Math.max(1, dp(0.5f)));
        paint.setStyle(Paint.Style.STROKE);
        paint.setColor(0x12EDBD68);
        canvas.drawCircle(getWidth() * 0.07f, getHeight() * 0.75f, dp(102), paint);
        canvas.drawCircle(getWidth() * 0.07f, getHeight() * 0.75f, dp(128), paint);
        paint.setColor(0x0FABBAC7);
        float edge = getWidth() - dp(90);
        for (int i = 0; i < 4; ++i) {
            float x = edge + dp(i * 22);
            canvas.drawLine(x, getHeight() * 0.55f, x, getHeight() + dp(24), paint);
        }
        canvas.drawLine(edge - dp(12), getHeight() * 0.55f, getWidth(), getHeight() * 0.55f, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(0xFFEDBD68);
        float center = getWidth() / 2f;
        canvas.drawRoundRect(center - dp(14), hero.getTop() - dp(13), center + dp(14),
                hero.getTop() - dp(11), dp(1), dp(1), paint);
    }
}
