import QtQuick

Canvas {
    id: root
    property string kind: "line"
    property color iconColor: MarchCraftTheme.textPrimary
    implicitWidth: 24; implicitHeight: 24
    onKindChanged: requestPaint(); onIconColorChanged: requestPaint()
    Component.onCompleted: requestPaint()
    onPaint: {
        const c=getContext("2d"); c.reset(); c.strokeStyle=iconColor; c.lineWidth=1.8; c.lineCap="round"; c.lineJoin="round"
        const w=width,h=height,p=4,x=w/2,y=h/2,r=Math.min(w,h)/2-p
        c.beginPath()
        if(kind==="line"){c.moveTo(p,h-p);c.lineTo(w-p,p)}
        else if(kind==="rectangle")c.rect(p,p,w-2*p,h-2*p)
        else if(kind==="circle")c.arc(x,y,r,0,Math.PI*2)
        else if(kind==="ellipse")c.ellipse(x,y,r,r*.62,0,0,Math.PI*2)
        else if(kind==="triangle"){c.moveTo(x,p);c.lineTo(w-p,h-p);c.lineTo(p,h-p);c.closePath()}
        else if(kind==="diamond"){c.moveTo(x,p);c.lineTo(w-p,y);c.lineTo(x,h-p);c.lineTo(p,y);c.closePath()}
        else if(kind==="arc")c.arc(x,y+3,r,Math.PI,Math.PI*2)
        else if(kind==="block"){c.rect(p,p,w-2*p,h-2*p);c.moveTo(x,p);c.lineTo(x,h-p);c.moveTo(p,y);c.lineTo(w-p,y)}
        else { const n=kind==="star"?10:6; for(let i=0;i<n;++i){const a=-Math.PI/2+i*Math.PI*2/n,rr=kind==="star"&&i%2?r*.45:r;const px=x+Math.cos(a)*rr,py=y+Math.sin(a)*rr;if(!i)c.moveTo(px,py);else c.lineTo(px,py)}c.closePath() }
        c.stroke()
    }
}
