// AI聊天消息数据类
export class ChatMessage {
  text: string
  isUser: boolean  // true表示用户消息，false表示AI回复
  timestamp: Date

  constructor(text: string, isUser: boolean) {
    this.text = text
    this.isUser = isUser
    this.timestamp = new Date()
  }
}

// 聊天消息组件
@Component
export struct ChatBubble {
  @Prop message: ChatMessage

  build() {
    Row() {
      // 显示用户头像或AI头像
      // Image(this.message.isUser ? $r('app.media.MY') : $r('app.media.XH'))
      //   .width(40)
      //   .height(40)
      //   .margin({ right: this.message.isUser ? 0 : '1%', left: this.message.isUser ? '1%' : 0 })

      // 消息文本
      Text(this.message.text)
        .constraintSize({ maxWidth: '80%', minWidth: '20%' })
        .fontSize(14)
        .fontColor(Color.Black)
        .backgroundColor(this.message.isUser ? '#C8E6C9' : '#E1F5FE')
        .borderRadius(10)
        .padding(10)
        .textAlign(TextAlign.Start)
        .margin({ left: 8, right: 8 })
    }
    .width('100%')
    .padding({ top: 5, bottom: 5 })
    .justifyContent(FlexAlign.Start)
    .direction(this.message.isUser ? Direction.Rtl : Direction.Ltr)
  }
}
